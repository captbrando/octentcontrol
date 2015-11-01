/******************************************************************
 * ( $Id: log.c,v 1.9 2003/04/02 18:12:47 brw Exp $ )
 *
 * log.c
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		Handle logging functions here.
 *
 * $Log: log.c,v $
 * Revision 1.9  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#include "log.h"

/* grab some globals */
extern short DEBUG;
extern char LOGFILENAME[150];
extern short LOG_TO_FILE;
extern short LOG_TO_SYSLOG;
extern short KEEP_IN_FOREGROUND;
extern short LOG_TO_DATABASE;
extern PGconn *myDbHandler;

void logEntry(char logType, char *logAction) {
    if (logAction) {
        if ((DEBUG && logType == MYLOG_DEBUG) || logType != MYLOG_DEBUG) {
            if (LOG_TO_FILE) {
                fprintf(myLogFile, "%s\n", logAction);
            }

            if (LOG_TO_SYSLOG) {
                if (logType == MYLOG_ERROR) {
                    syslog(LOG_ERR, "%s", logAction);
                } else {
                    syslog(LOG_INFO, "%s", logAction);
                }
            }

            // Do we really need to check for a database connection?  We crap out if there isn't one.
            if (LOG_TO_DATABASE) {
                if (myDbHandler) {
                    char dbQuery[600], myLogAction[600];
                    PQescapeString (myLogAction, logAction, strlen(logAction));
                    snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO log (logtype,action) VALUES ('%c','%s')", logType, myLogAction);
                    myDbSelect(dbQuery);
                }
            }
        }
    }
}

void openLog() {
	if (LOG_TO_FILE) {
		myLogFile = fopen(LOGFILENAME, "a");
	}
	if (LOG_TO_SYSLOG) {
		openlog("octentcontrol", LOG_PID, LOG_USER);
	}
}

void closeLog() {
	if (LOG_TO_FILE) {
		fclose(myLogFile);
	}
	if (LOG_TO_SYSLOG) {
		closelog();
	}
}
