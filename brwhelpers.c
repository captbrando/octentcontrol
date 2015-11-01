/******************************************************************
 * ( $Id: brwhelpers.c,v 1.20 2003/04/10 17:49:21 brw Exp $ )
 * brwhelpers.c
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *	A few helper functions that really dont belong in main.c.
 *	
 * $Log: brwhelpers.c,v $
 * Revision 1.20  2003/04/10 17:49:21  brw
 * Changed some logging...
 *
 * Revision 1.19  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 * Revision 1.18  2003/04/02 17:54:50  brw
 * changed name to octentcontrol
 *
 *
 ******************************************************************/

#include "brwhelpers.h"

/* some globs */
extern int sleeptime;
extern int numthreads;

int splitIpAddress(char *ip_ptr, short *first, short *second, short *third, short *fourth) {
	char *fourth_ptr, *first_ptr, *second_ptr, *third_ptr;
	/* now this is not the most efficient ever.  Optimize this later... */

	first_ptr = ip_ptr;
	second_ptr = strchr(ip_ptr, '.');

	if (second_ptr == NULL) {
		return 1;
	}
	*second_ptr = '\0';
	second_ptr++;
	third_ptr = strchr(second_ptr, '.');

	if (third_ptr == NULL) {
		return 1;
	}
	*third_ptr = '\0';
	third_ptr++;
	fourth_ptr = strchr(third_ptr, '.');
	if (fourth_ptr == NULL) {
		return 1;
	}
	*fourth_ptr = '\0';
	fourth_ptr++;

	*first = atoi(first_ptr);
	*second = atoi(second_ptr);
	*third = atoi(third_ptr);
	*fourth = atoi(fourth_ptr);

	return 0;
}

void sigControl(int sig) {
	/* ignore a duplicate */
	signal(sig, SIG_IGN);

	/* Handle a SIGTERM or SIGINT */
	if (sig == SIGTERM || sig == SIGINT) {
		logEntry(MYLOG_DEBUG,"We got a SIGTERM.  Cleaning up.");
		control_deactivate(&wq.control);
		/* CLEANUP  */
		join_threads();
		cleanup_structs();
		logEntry(MYLOG_DEBUG,"Exiting....");
		exit(0);
	}

	/* Handle a SIGPIPE */
	if (sig == SIGPIPE) {
		logEntry(MYLOG_ERROR,"We got a SIGPIPE.  The Database connection was terminated.  Cleaning up.");
		control_deactivate(&wq.control);
		/* CLEANUP  */
		join_threads();
		cleanup_structs();
		logEntry(MYLOG_DEBUG,"Exiting....");
		exit(0);
	}
}

void populateEntries (int myJobNum, int myServerID) {
	short beg_first, beg_second, beg_third, beg_fourth, RECOVER=0;
	short end_first, end_second, end_third, end_fourth;
	int x;
	char myIPAddress[75];
	char dbQuery[250];
	wnode *mywork;
	PGresult *myResult, *myRowResult;

	/* ok, the spec has changed.  On this end, we are not going to be concerned with such things
	 * like determining the network range etc.  We are simply going to be reading from a node
	 * table.  This way we can also recover from a crash in the middle of a scan instead
	 * of starting over.
	 */

	snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM nodequeue WHERE scanid = %d", myJobNum);
	logEntry(MYLOG_DEBUG, "Checking to see if we have inserted this scan before...");
	logEntry(MYLOG_DEBUG, dbQuery);
	myResult = myDbSelect(dbQuery);
	if (PQntuples(myResult) > 0) {
		logEntry(MYLOG_DEBUG, "We have inserted this scanid before, we must be recovering...");
		RECOVER = 1;
	}
	PQclear(myResult);
	snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM targetconf WHERE scanid = %d", myJobNum);
	logEntry(MYLOG_DEBUG, dbQuery);
	myRowResult = myDbSelect(dbQuery);

	// first check to see if there are any entries in targetconf.  if not, change status to 'n'
	if (PQntuples(myRowResult) == 0) {
		snprintf(dbQuery, sizeof(dbQuery), "UPDATE scanqueue SET status = 'n' WHERE scanid = %d", myJobNum);
		logEntry(MYLOG_DEBUG, "Thats strange, there were no items in targetconf!");
		logEntry(MYLOG_DEBUG, dbQuery);
		myDbSelect(dbQuery);
		return;
	}

	for (x=0; x < PQntuples(myRowResult); x++) {
		if (!strncmp(PQgetvalue(myRowResult, x, 2), "i", 1)) {
			// we have a single host.  Just dump it in there.
			/* if we are not in recover mode, we need to set some junk in the db */
			if (!RECOVER) {
				snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO nodequeue (scanid,nodeaddress) VALUES (%d,'%s')", myJobNum, PQgetvalue(myRowResult, x, 3));
				logEntry(MYLOG_DEBUG, dbQuery);
				myDbSelect(dbQuery);
				snprintf(dbQuery, sizeof(dbQuery), "UPDATE scanqueue SET startdate = NOW(), status='p' where scanid = %d", myJobNum);
				logEntry(MYLOG_DEBUG, dbQuery);
				myDbSelect(dbQuery);
			} else {
				// first fix anything that was in process during the crash to queued.
				snprintf(dbQuery, sizeof(dbQuery), "UPDATE nodequeue SET status='n' where scanid = %d AND status in ('p','q')", myJobNum);
				logEntry(MYLOG_DEBUG, dbQuery);
				myDbSelect(dbQuery);
				snprintf(dbQuery, sizeof(dbQuery), "UPDATE scanqueue SET status='p' where scanid = %d", myJobNum);
				logEntry(MYLOG_DEBUG, dbQuery);
				myDbSelect(dbQuery);

				//just check to see if this entry is in the nodequeue table real quick.
				snprintf(dbQuery, sizeof(dbQuery), "SELECT count(*) FROM nodequeue WHERE nodeaddress like '%s' AND scanid = %d", PQgetvalue(myRowResult, x, 3), myJobNum);
				logEntry(MYLOG_DEBUG, dbQuery);
				myResult = myDbSelect(dbQuery);
				if (PQntuples(myResult) == 0) {
					snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO nodequeue (scanid,nodeaddress) VALUES (%d,'%s')", myJobNum, PQgetvalue(myResult, x, 3));
					logEntry(MYLOG_DEBUG, dbQuery);
					myDbSelect(dbQuery);
				}
				PQclear(myResult);
			}
		} else {
			if (RECOVER) {
				snprintf(dbQuery, sizeof(dbQuery), "UPDATE nodequeue SET status='n' where scanid = %d and status = 'p'", myJobNum);
				logEntry(MYLOG_DEBUG, dbQuery);
				myDbSelect(dbQuery);
			} else {
				snprintf(dbQuery, sizeof(dbQuery), "UPDATE scanqueue SET startdate = NOW(), status='p' where scanid = %d", myJobNum);
				logEntry(MYLOG_DEBUG, dbQuery);
				myDbSelect(dbQuery);

				// here we populate the nodequeue table....
				if (splitIpAddress(PQgetvalue(myRowResult, x, 4), &beg_first, &beg_second, &beg_third, &beg_fourth)) {
					logEntry(MYLOG_ERROR,"Could not find enough . in beginning IP.  Skipping...");
				}
				if (splitIpAddress(PQgetvalue(myRowResult, x, 5), &end_first, &end_second, &end_third, &end_fourth)) {
					logEntry(MYLOG_ERROR,"Could not find enough . in beginning IP.  Skipping...");
				}

				while ((beg_first < end_first) || (beg_second < end_second) || (beg_third < end_third) || (beg_fourth <= end_fourth)) {
					// check to see if we are outside the bounds of a class C.
					if (beg_fourth == 256) {
						beg_third++;
						beg_fourth = 1;
					}
					if (beg_third == 256) {
						beg_second++;
						beg_third = 0;
					}
					if (beg_second == 256) {
						beg_first++;
						beg_second = 0;
					}
					if (beg_fourth != 255) {
						snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO nodequeue (scanid,nodeaddress) VALUES (%d,'%d.%d.%d.%d')", myJobNum, beg_first, beg_second, beg_third, beg_fourth);
						logEntry(MYLOG_DEBUG, dbQuery);
						myDbSelect(dbQuery);
					}
					beg_fourth++;
				}
			}
		}
	}

	PQclear(myRowResult);
	// select out of DB second and malloc() then.
	snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM nodequeue WHERE scanid = %d AND status = 'n'", myJobNum);
	logEntry(MYLOG_DEBUG, dbQuery);
	myResult = myDbSelect(dbQuery);
	for (x=0; x < PQntuples(myResult); x++) {
		mywork=malloc(sizeof(wnode));
		if (!mywork) {
			logEntry(MYLOG_ERROR, "D'oh! can't malloc!\n");
			break;
		}
		mywork->jobnum = myJobNum;
		snprintf(myIPAddress, sizeof(myIPAddress), "%s", PQgetvalue(myResult, x, 3));
		mywork->scanningServerID = myServerID;

		strncpy(mywork->networknode, myIPAddress, min(sizeof(mywork->networknode),sizeof(myIPAddress)-1));
		pthread_mutex_lock(&wq.control.mutex);
		queue_put(&wq.work,(node *) mywork);
		pthread_mutex_unlock(&wq.control.mutex);
	}
	PQclear(myResult);
}

void requeueKilledServerWork (int myServerID) {
	int x;
	char myIPAddress[75];
	char dbQuery[250];
	wnode *mywork;
	PGresult *myResult;

	snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM nodequeue WHERE serverid = %d AND status = 'p'", myServerID);
	logEntry(MYLOG_DEBUG, "Re-Entering work into the queue...");
	logEntry(MYLOG_DEBUG, dbQuery);
	myResult = myDbSelect(dbQuery);

	for (x=0; x < PQntuples(myResult); x++) {
		mywork=malloc(sizeof(wnode));
		if (!mywork) {
			logEntry(MYLOG_ERROR, "D'oh! can't malloc!\n");
			break;
		}
		mywork->jobnum = atoi(PQgetvalue(myResult, x, 2));
		snprintf(myIPAddress, sizeof(myIPAddress), "%s", PQgetvalue(myResult, x, 3));
		mywork->scanningServerID = 0;

		strncpy(mywork->networknode, myIPAddress, min(sizeof(mywork->networknode),sizeof(myIPAddress)-1));
		pthread_mutex_lock(&wq.control.mutex);
		queue_put(&wq.work,(node *) mywork);
		pthread_mutex_unlock(&wq.control.mutex);
	}
	PQclear(myResult);
}

void recoverFromCrash(void) {
	PGresult *myrfcResult;
	char dbQuery[250];
	int x;

	/* if we crashed or terminated abnormally, we need to update all items that were
	 * in process at the time and rescan or something. */
	snprintf(dbQuery, sizeof(dbQuery), "SELECT scanid,serverid FROM scanqueue WHERE finishdate IS NULL AND status='p' AND startdate IS NOT NULL");
	logEntry(MYLOG_DEBUG, "Checking to see if we crashed...");
	logEntry(MYLOG_DEBUG, dbQuery);
	myrfcResult = myDbSelect(dbQuery);

	if (PQntuples(myrfcResult) > 0) {
		for (x=0; x < PQntuples(myrfcResult); x++) {
			populateEntries(atoi(PQgetvalue(myrfcResult, x, 0)), atoi(PQgetvalue(myrfcResult, x, 1)));
		}
	}
	PQclear(myrfcResult);
	pthread_cond_broadcast(&wq.control.cond);
	logEntry(MYLOG_DEBUG,"done recovering...\n");
}

void checkForWork(void) {
	PGresult *mycwResult;
	int x;

	logEntry(MYLOG_DEBUG, "Waking up and checking for new items to scan...");
	logEntry(MYLOG_DEBUG, "SELECT scanid,serverid FROM scanqueue WHERE status='q' OR (status = 's' AND scheduledate < NOW())");
	mycwResult = myDbSelect("SELECT scanid,serverid FROM scanqueue WHERE status='q' OR (status = 's' AND scheduledate < NOW())");

	/*
	 * Ok things have changed a bit.  Now we need to check the scanqueue for work.  If
	 * we find some, we must select entries from the node table.  Now those entries can
	 * either be single hosts (i), or ranges (r) in which we need to take the beginning
	 * address, and ending address.  Avoid using a 0 or 255 in the 4th octet, and simply
	 * populate all the entries to the work list.
	 */

	if (PQntuples(mycwResult) > 0) {
		for (x=0; x < PQntuples(mycwResult); x++) {
			populateEntries(atoi(PQgetvalue(mycwResult, x, 0)), atoi(PQgetvalue(mycwResult, x, 1)));
		}
	}
	PQclear(mycwResult);
	pthread_cond_broadcast(&wq.control.cond);	// hrm should that be in the result if?
	logEntry(MYLOG_DEBUG,"control thread sleeping...");
	sleep(sleeptime);
}

void checkForMessages(void) {
	PGresult *mycmResult;
	char dbQuery[250], myServerID[10], message[250];
	int x,y;

	snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM messagequeue WHERE ack=0 AND msgTo = 'n'");
	logEntry(MYLOG_DEBUG, "Looking for messages from the frontend.");
	logEntry(MYLOG_DEBUG, dbQuery);
	mycmResult = myDbSelect(dbQuery);

	if (PQntuples(mycmResult) > 0) {
		// parse our messages
		for (x=0; x < PQntuples(mycmResult); x++) {
			strncpy(message,PQgetvalue(mycmResult,x,5), min(sizeof(message),strlen(PQgetvalue(mycmResult,x,5))));

			switch(message[0]) {
				case 'k':
					// Here we kill threads
					// somehow get some sort of global going so that when threads wake up,
					// first they check to see if they need to add themselves to the cleanup
					// crew.  work on that.....
					y=1;
					while(message[y]) {
						myServerID[y-1] = message[y];
						y++;
					}

                    killThreads(&tq.thread,atoi(myServerID));
					requeueKilledServerWork(atoi(myServerID));

					// ok this is not the best way to do it.  For those threads that are
					// actively scanning, there is a good chance they will not see the
					// die message, ESPECIALLY if someone goes on a killing spree.
					// One of these days I'll need to see about maintaining thread control
					// while the system() command is running.  Maybe with an exec, and then
					// just check for my results file.  who knows.

					snprintf(dbQuery, sizeof(dbQuery), "Roger.  Backend killed threads for server id %d", atoi(myServerID));
					logEntry(MYLOG_DEBUG, dbQuery);
					break;

				case 's':
					// Here we start threads
					y=1;
					while(message[y]) {
						myServerID[y-1] = message[y];
						y++;
					}
					myServerID[y-1]='\0';

					create_threads(atoi(myServerID));
					snprintf(dbQuery, sizeof(dbQuery), "Roger.  Created threads for server id %d.", atoi(myServerID));
					logEntry(MYLOG_DEBUG, dbQuery);
					break;

				case 'r':
					// Here we restart octentcontrol.
					// not sure how we are gonna do that...
					// probably gonna have to reread config, kill all threads, then start over.
					break;

				case 'd':
					sigControl(SIGTERM);
					break;

				default:
					snprintf(dbQuery, sizeof(dbQuery), "Got a message I did not understand: (id %d) %s", atoi(PQgetvalue(mycmResult,x,0)),message);
					logEntry(MYLOG_DEBUG, dbQuery);
/*					snprintf(dbQuery, sizeof(dbQuery), "insert into messagequeue (msgto, msgfrom, message) values ('w','n','What?  Did not understand the last message (id %d).')", atoi(PQgetvalue(mycmResult,x,0)));
					logEntry(MYLOG_DEBUG, dbQuery);
					myDbSelect(dbQuery);
*/					break;
			}

			snprintf(dbQuery, sizeof(dbQuery), "UPDATE messagequeue SET ack=1 WHERE messageid = %d",atoi(PQgetvalue(mycmResult,x,0)));
			logEntry(MYLOG_DEBUG, dbQuery);
			mycmResult = myDbSelect(dbQuery);
		}
	}
	PQclear(mycmResult);
	logEntry(MYLOG_DEBUG,"done checking for messages...");
}
