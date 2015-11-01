/******************************************************************
 * ( $Id: main.c,v 1.37 2003/04/02 18:12:47 brw Exp $ )
 *
 * main.c (OCTENTCONTROL)
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		Main program code for our backend app.
 *
 * Recent Changes...
 * 
 * $Log: main.c,v $
 * Revision 1.37  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 * Revision 1.36  2003/04/02 17:54:50  brw
 * changed name to octentcontrol
 *
 *		
 ******************************************************************/

#include "main.h"

/* Define some global variables here so we dont run into multiple definition problems
   Some of these items are defaults that can be changed based on items read. */
const char VERSION[20] = "1.0.0 RC1";
short DEBUG = 0;
char LOGFILENAME[150];
char INSTALL_DIR[250];
short LOG_TO_FILE = 0;
short LOG_TO_SYSLOG = 0;
short KEEP_IN_FOREGROUND = 1;
short LOG_TO_DATABASE = 1;
short KEEP_NESSUS_DRIVE_FILES = 0;
int SIMULATE = 0;
extern PGconn *myDbHandler;
extern char DB_HOST[80], DB_USER[80], DB_PASS[80], DB_DATABASE[80], DB_PORT[6];
int sleeptime = 360;  // 5 minutes
extern pthread_t cleanupThreadID;

void initialize_structs(void) {
	if (control_init(&wq.control))
		detailabort();
	queue_init(&wq.work);
	if (control_init(&cq.control)) {
		control_destroy(&wq.control);
		detailabort();
	}
	queue_init(&cq.cleanup);
	control_activate(&wq.control);
	control_activate(&cq.control);
}

void cleanup_structs(void) {
	control_destroy(&cq.control);
	control_destroy(&wq.control);
}

void printUsage(void) {
	fprintf(stderr, "OctentControl version %s\nUsage: octentcontrol <flags>\n\n", VERSION);
	fprintf(stderr, "Flags for use: \n");
	fprintf(stderr, "\t-d\t\tLog a ton of debugging information.\n");
	fprintf(stderr, "\t-k\t\tDo not delete target/results files nessus uses.\n");
	fprintf(stderr, "\t-l <filename>\tLog to <filename>.\n");
	fprintf(stderr, "\t-s\t\tLog to syslog.\n");
	fprintf(stderr, "\t-b\t\tTurn off database logging.\n");
	fprintf(stderr, "\t-F\t\tFork() to the background.\n");
	fprintf(stderr, "\t-S\t\tJust simulate what would happen, do not run nessus.\n");
}

void readConfiguration(void) {
	FILE *configFile;
	char buf[80], *directive, *value, *tmp;

	configFile = fopen("octentcontrol.conf","r");

	/* go line by line.  If lines start with #, skip it */
    /* also if the length of the string fetched is not greater than 4, skip it */
	if (configFile) {
		while (fgets(buf, 80, configFile)) {
			if (buf[0] != '#' && strlen(buf) > 4) {
				directive = buf;
				value = strchr(buf, '=');

				if (value == NULL) {
					fprintf(stderr, "Error parsing config file... exiting\n");
					exit(1);
				}
				*value = '\0';
				value++;

				// Strip the newline
				tmp = strchr(value, '\n');
				if (tmp != NULL) {
					*tmp = '\0';
				}

				if (!strcmp("DB_HOST",directive)) {
					strncpy(DB_HOST, value, sizeof(DB_HOST));
				}
				if (!strcmp("DB_USER",directive)) {
					strncpy(DB_USER, value, sizeof(DB_USER));
				}
				if (!strcmp("DB_PASS",directive)) {
					strncpy(DB_PASS, value, sizeof(DB_PASS));
				}
				if (!strcmp("DB_PORT",directive)) {
					strncpy(DB_PORT, value, sizeof(DB_PORT));
				}
				if (!strcmp("DB_DATABASE",directive)) {
					strncpy(DB_DATABASE, value, sizeof(DB_DATABASE));
				}
				if (!strcmp("SLEEP_MINUTES",directive)) {
					sleeptime = atoi(value) * 60;
				}
				if (!strcmp("INSTALL_DIR",directive)) {
					strncpy(INSTALL_DIR, value, sizeof(INSTALL_DIR));
				}
			}
		}
	} else {
        fprintf(stderr, "Could not open configuration file.\n");
        exit(2);
    }
	fclose(configFile);
}

int main(int argc, char *argv[]) {
	short myPid,i;

	/* Get arguments from the command line */
	for (i = 1; i < argc; i++) {
		switch(argv[i][1]) {
			case 'd':	/* Debug a lot */
					DEBUG = 1;
					break;
			case 'l':	/* Grab a file name to log to */
					i++;
					if (!argv[i]) {
						printUsage();
						exit(0);
					}
					strncpy(LOGFILENAME, argv[i], min(sizeof(LOGFILENAME)-1, strlen(argv[i])));
					LOG_TO_FILE = 1;
					break;
			case 's':	/* Log to syslog */
					LOG_TO_SYSLOG = 1;
					break;
			case 'S':	/* Just simulate */
					SIMULATE = 1;
					break;
			case 'k':	/* keep those results/target files */
					KEEP_NESSUS_DRIVE_FILES = 1;
					break;
			case 'F':	/* fork() to background */
					KEEP_IN_FOREGROUND = 0;
					break;
			case 'b':	/* do not log to database */
					LOG_TO_DATABASE = 0;
					break;
			default:	 /* I did not recognize the argument, print usage. */
					printUsage();
					exit(0);
					break;
		}
	}

	if (argc == 1) {
		printUsage();
		exit(0);
	}

	/* open our log file if needed */
	openLog();

	/* ok, just throw ourselves into the background.  NOW! */
	if (!KEEP_IN_FOREGROUND) {
		myPid = fork();
		if (myPid == -1) {
			logEntry(MYLOG_ERROR, "Yipes, I die now.  fork() failed.");
			exit(1);
		} else if (myPid > 0) {
			logEntry(MYLOG_ERROR, "fork() succeeded, see you in the background.");
			exit(0);
		}
	}

	/* read our config in */
	readConfiguration();

	/* initialize the database */
	if (myDbConnect()) {
		fprintf(stderr,"Error connecting to the database... cleaning up.\n");
		closeLog();
        exit(1);
	}

	/* Initialize my data structures */
	initialize_structs();

	/* instantiate the cleanup thread. */
	if (pthread_create(&cleanupThreadID, NULL, cleanupthread, NULL)) {
		logEntry(MYLOG_ERROR,"Error starting cleanup thread... cleaning up.");
		closeLog();
        exit(1);
	}

	/* instantiate the threads.  We know how many from the DB. */
	if (create_threads(0)) {
		logEntry(MYLOG_ERROR,"Error starting threads... cleaning up.");
		join_threads();
		closeLog();
        exit(1);
	}

	/* Trap SIGTERM, SIGINT.  The correct way to kill this is kill -TERM <pid> */
	signal(SIGTERM, sigControl);
	signal(SIGINT, sigControl);
	signal(SIGPIPE, sigControl);

	/* first check to see if any jobs did not complete. */
	recoverFromCrash();

	/* now get have us some phun */
	for(;;) {
		checkForWork();
		checkForMessages();
	}
	// for sanity's sake, we do the next 2 lines.  We never should see it.
    sigControl(SIGTERM);
	return 0;
}
