/******************************************************************
 * ( $Id: mypthreads.c,v 1.34 2003/10/13 15:46:32 brw Exp $ )
 * mypthreads.c
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		All of the threading code.
 * 
 * $Log: mypthreads.c,v $
 * Revision 1.34  2003/10/13 15:46:32  brw
 * fixed some memory leaks
 *
 * Revision 1.33  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#include "mypthreads.h"

int numthreads = 0;
int threadIdSeed = 0;
extern PGconn *myDbHandler;
extern int SIMULATE;
extern int sleeptime;
extern short DEBUG;
extern short KEEP_NESSUS_DRIVE_FILES;
pthread_mutex_t xmlParsingMutex;
pthread_mutex_t nessusInstantiationMutex;
pthread_t cleanupThreadID;

/* Declare some globals that are used in the XML processing... */
struct basedata {
	char hostname[50],ip[50];
	char startdate[30],enddate[30];
	int myJobNumber;
} myBaseData;

struct port {
	char portnumber[5],protocol[5];
	char servicename[20];
	char servicemethod[10],serviceconf[5];  //These always seem to be "nessus" and "3" respectively.
	char severity[30],id[10];
	char portdata[8196];
} myPort;
int hostcount = 1, startcount = 1, endcount = 1, reportid = 0, currentLength=0;
char CURRENTTAG[100];  // only so I can tell what the current tag is if attributes AND elements are used.


void *scanthread(void *myarg) {
	wnode *mywork;
	cnode *mynode;
	time_t ltime;
	char targetfilename[80], resultsfilename[80], commandToRun[300], dbQuery[250], myTarget[50];
	FILE *myTargetFile, *myResultsFile;
	PGresult *myResult;
	int myJobNumber,weHaveFailure,numFailures;

	mynode=(cnode *) myarg;

	pthread_mutex_lock(&wq.control.mutex);
	while (wq.control.active) {
		while (wq.work.head==NULL && wq.control.active) {
			pthread_cond_wait(&wq.control.cond, &wq.control.mutex);
		}
		if (!wq.control.active)
			break;

		/* If for some reason we did not grab the info first, exit */
		/* Now that we have some work to do, lets do it */
		mywork=(wnode *) smart_queue_get(&wq.work, mynode->scanningServerID, mynode->overflow);
		pthread_mutex_unlock(&wq.control.mutex);

		if (mywork != NULL) {
			snprintf(dbQuery, sizeof(dbQuery), "UPDATE nodequeue SET status = 'p', serverid = %d where nodeaddress = '%s'", mynode->scanningServerID, mywork->networknode);
			logEntry(MYLOG_DEBUG, dbQuery);
			myDbSelect(dbQuery);
		}

		/* since we have this issue of "we only do work if we can based on prefs and stuff",
			if a NULL pointer is returned, we should simply skip ahead and sleep.
		*/
		if (mywork != NULL) {
			weHaveFailure=1;
            numFailures=0;
			myJobNumber = mywork->jobnum;

			snprintf(dbQuery, sizeof(dbQuery), "SELECT scanconf.scanfilename FROM scanqueue,scanconf WHERE scanqueue.scanid = %d AND scanqueue.scanconfid = scanconf.scanconfid", myJobNumber);
			logEntry(MYLOG_DEBUG, dbQuery);
			myResult = myDbSelect(dbQuery);

			if (PQntuples(myResult) == 0) {
				logEntry(MYLOG_DEBUG, "Uh oh, no scanid?  Thats bad news.");
            } else {
                ;

				strncpy(myTarget, mywork->networknode, min(sizeof(myTarget), sizeof(mywork->networknode))-1);
				/* Ok just get the seconds since EPOCH, add the thread number at the end and
				   whammo.  we gots us a unique filename */
				time(&ltime);
				snprintf(targetfilename, sizeof(targetfilename), "targets/target.%ld.%d.ncfile", ltime, mynode->threadnum);
				snprintf(resultsfilename, sizeof(resultsfilename), "results/results.%ld.%d.ncfile", ltime, mynode->threadnum);
				snprintf(commandToRun, sizeof(commandToRun), "/usr/local/bin/nessus -q --output-type=xml --config-file=%s --batch-mode %s 1241 %s %s %s %s\n", PQgetvalue(myResult, 0, 0), mynode->scanningServerAddy, mynode->scanningServerUsername, mynode->scanningServerPassword, targetfilename, resultsfilename);
				// Do the processing...

				myTargetFile = fopen(targetfilename, "w");
				fprintf(myTargetFile, "%s\n", myTarget);
				fclose(myTargetFile);

			}

			if (SIMULATE) {
				logEntry(MYLOG_DEBUG,"System call is run now...  Sleeping 20....");
				sleep(20);
			} else {
				while(weHaveFailure) {
					// Ok this looks really funny, but let me explain.  I need to offset my
                    // command line runs.  So I obtain a lock on a mutex, and then sleep for 4
                    // seconds.  this allows me to stagger my runs and is much more processor
                    // and server friendly.
					pthread_mutex_lock(&nessusInstantiationMutex);
			        sleep(10);
					pthread_mutex_unlock(&nessusInstantiationMutex);

					logEntry(MYLOG_DEBUG,"Trying to run system() call now...");
					logEntry(MYLOG_DEBUG, "RUNNING SHELL CMD!");
					logEntry(MYLOG_DEBUG, commandToRun);

					if (system(commandToRun)) {
						logEntry(MYLOG_ERROR,"**** oh crap.  the system() call failed.  Gonna sleep and try again. ****");
						weHaveFailure=1;
                        numFailures++;
                        if (numFailures == 3) {
                            break;  // simply, if we fail 3 times, something must be badly wrong.  close this thread and stop wasting time.
                        }
						sleep(sleeptime);
					} else {
						/* Oooook.  first, lets check and see if it Actually completed.
						   if it did, we should have a results file. */
						myResultsFile = fopen(resultsfilename,"r");
						if(myResultsFile) {
							fclose(myResultsFile);
							weHaveFailure=0;
						} else {
							/* Ok, generally bad things happened when we get here.

							   Here's what will happen.  We put our work BACK on the
							   queue (look down about 10 lines...).  and we also should join
							   this thread because something is very apparently wrong.
							*/
                            break;
                        }
					}
				}
                if (weHaveFailure) {
					logEntry(MYLOG_DEBUG,"thread shutting down because nessus crapped out...");
					snprintf(commandToRun, sizeof(commandToRun), "How sad, we only have %d threads left.", numthreads);
					logEntry(MYLOG_DEBUG,commandToRun);
    				pthread_mutex_lock(&wq.control.mutex);
					queue_put(&wq.work,(node *) mywork); // put work back on queue
					pthread_mutex_unlock(&wq.control.mutex);
					pthread_cond_signal(&wq.control.cond);
					pthread_mutex_lock(&cq.control.mutex);
					queue_put(&cq.cleanup,(node *) mynode);
					pthread_mutex_unlock(&cq.control.mutex);
					pthread_cond_signal(&cq.control.cond);
					return NULL;
                }
			}

			if (!KEEP_NESSUS_DRIVE_FILES) {
				unlink(targetfilename);
			}

			myResultsFile = fopen(resultsfilename,"a");
			fprintf(myResultsFile, "\n");
			fclose(myResultsFile);

			if (!SIMULATE) {
				XML_Parser p = XML_ParserCreate(NULL);
				char Buff[512];

				/* obtain the mutex lock here so we can have some fun. */
				pthread_mutex_lock(&xmlParsingMutex);

				// Now clean up our variables...
		   	  	memset(myBaseData.hostname,'\0',sizeof(myBaseData.hostname));
	  		 	memset(myBaseData.ip,'\0',sizeof(myBaseData.ip));
	  		 	memset(myBaseData.startdate,'\0',sizeof(myBaseData.startdate));
	  		 	memset(myBaseData.enddate,'\0',sizeof(myBaseData.enddate));
	  		 	memset(myPort.portnumber,'\0',sizeof(myPort.portnumber));
	  		 	memset(myPort.protocol,'\0',sizeof(myPort.protocol));
	  		 	memset(myPort.servicename,'\0',sizeof(myPort.servicename));
	  		 	memset(myPort.servicemethod,'\0',sizeof(myPort.servicemethod));
	  		 	memset(myPort.serviceconf,'\0',sizeof(myPort.serviceconf));
	  		 	memset(myPort.id,'\0',sizeof(myPort.id));
	  		 	memset(myPort.severity,'\0',sizeof(myPort.severity));
	  		 	memset(myPort.portdata,'\0',sizeof(myPort.portdata));
				hostcount=1;
				startcount=1;
				endcount=1;
				reportid=0;
				currentLength=0;
				myBaseData.myJobNumber = myJobNumber;

				strcpy(CURRENTTAG,"none");

				myResultsFile = fopen(resultsfilename,"r");

				snprintf(Buff, sizeof(Buff), "Attempting to parse results file: %s", resultsfilename);
				logEntry(MYLOG_ERROR, Buff);
				if (!myResultsFile) {
					snprintf(Buff, sizeof(Buff), "Could not open results file: %s", resultsfilename);
					logEntry(MYLOG_ERROR, Buff);
					break;
				}

				if (! p) {
					logEntry(MYLOG_ERROR,"Couldn't allocate memory for XML parser.");
					break;
				}

				XML_UseParserAsHandlerArg(p);
				XML_SetElementHandler(p, start_hndl, end_hndl);
				XML_SetCharacterDataHandler(p, char_hndl);

				for (;;) {
					int done;
					int len;

					fgets(Buff, sizeof(Buff), myResultsFile);
					len = strlen(Buff);
					if (ferror(myResultsFile)) {
						logEntry(MYLOG_ERROR,"File read error.");
						break;
					}
					done = feof(myResultsFile);
					if (done) {
						fclose(myResultsFile);
						if (!KEEP_NESSUS_DRIVE_FILES) {
							unlink(resultsfilename);
						}
						break;
					}
					if (! XML_Parse(p, Buff, len, done)) {
						fprintf(stderr, "Parse error at line %d:\n%s\n",
						XML_GetCurrentLineNumber(p),
						XML_ErrorString(XML_GetErrorCode(p)));
						exit(-1);
					}
				}
				pthread_mutex_unlock(&xmlParsingMutex);
			} /* xml parsage */

			PQclear(myResult);
			snprintf(dbQuery, sizeof(dbQuery), "UPDATE nodequeue SET status = 'c' where nodeaddress = '%s'", mywork->networknode);
			logEntry(MYLOG_DEBUG, dbQuery);
			myResult = myDbSelect(dbQuery);

			PQclear(myResult);
			snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO licensing (scanid,nodeaddress) VALUES (%d,'%s')",myJobNumber, mywork->networknode);
			logEntry(MYLOG_DEBUG, dbQuery);
			myResult = myDbSelect(dbQuery);

			// see if there are any more items either processing or in queue.
			snprintf(dbQuery, sizeof(dbQuery), "SELECT status FROM nodequeue WHERE status in ('q','p','n') AND scanid = %d", myJobNumber);
			logEntry(MYLOG_DEBUG, dbQuery);
			PQclear(myResult);
			myResult = myDbSelect(dbQuery);
			if (PQntuples(myResult) == 0) {
				snprintf(dbQuery, sizeof(dbQuery), "UPDATE scanqueue SET status = 'c', finishdate = NOW() WHERE scanid = %d", myJobNumber);
				logEntry(MYLOG_DEBUG, dbQuery);
				PQclear(myResult);
				myResult = myDbSelect(dbQuery);

				/* if conditions exist, copy **CHECK THAT, MOVE** that to the finishedscans table... */
				// This table may go away...
				snprintf(dbQuery, sizeof(dbQuery), "DELETE FROM nodequeue WHERE scanid = %d", myJobNumber);
				logEntry(MYLOG_DEBUG, dbQuery);
				PQclear(myResult);
				myResult = myDbSelect(dbQuery);
				snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO finishedscans (scanid,customerid,userid,scanconfid,scheduledate,startdate,finishdate,serverid) SELECT scanid,customerid,userid,scanconfid,scheduledate,startdate,finishdate,%d as serverid FROM scanqueue where status='c' AND scanid = %d", mynode->scanningServerID, myJobNumber);
				logEntry(MYLOG_DEBUG, dbQuery);
				PQclear(myResult);
				myResult = myDbSelect(dbQuery);
/*				snprintf(dbQuery, sizeof(dbQuery), "DELETE FROM scanqueue WHERE scanid = %d", myJobNumber);
				logEntry(MYLOG_DEBUG, dbQuery);
				PQclear(myResult);
				myResult = myDbSelect(dbQuery);
*/			}

			sleep(1); /* just make sure we are not blazing fast and create race conditions (see the file name)*/
			free(mywork);
			logEntry(MYLOG_DEBUG, "Thread finished job and sleeping...");
		} else {
			logEntry(MYLOG_DEBUG, "Thread sleeping.  There is work in the queue, but based on my setup I will not do any...");
			sleep (sleeptime);
		}
		pthread_mutex_lock(&wq.control.mutex);
	}
	pthread_mutex_unlock(&wq.control.mutex);
	pthread_mutex_lock(&cq.control.mutex);
	queue_put(&cq.cleanup,(node *) mynode);
	pthread_mutex_unlock(&cq.control.mutex);
	pthread_cond_signal(&cq.control.cond);
	logEntry(MYLOG_DEBUG,"thread shutting down...");
	return NULL;
}

/* The following 3 functions are specific to the XML parsing code. */
void start_hndl(void *data, const char *el, const char **attr) {
	/*
		I will be the first to admit this is a quite inefficient and rediculous
		way to do this.  The XML generated by Nessus does not exactly follow
		the DTD supplied, so I used the expat library.  There are duplications in the
		XML report which have to be accounted for.  Mainly because there are good/bad
		data in some of those tags (start/end for one).  Additionally, the XML report
		contains the scan configuration with no apparent way to turn it off.  So yes,
		we all can make excuses......  there's mine.
	*/
	// sheesh this is nuts.  there are 2 host tags.  So we take the second one.
	if (!strcmp(el,"host")) {
		if (hostcount == 2) {
 			strncpy(myBaseData.hostname,attr[1],min(sizeof(myBaseData.hostname),strlen(attr[1])));
			strncpy(myBaseData.ip,attr[3],min(sizeof(myBaseData.ip),strlen(attr[3])));
	   		strcpy(CURRENTTAG, "host");
		} else {
			hostcount++;
		}
	}

	// 2 start/end tags.  the first one has an extra &lt; in it.  dont want that one.
	if (!strcmp(el,"start")) {
		if (startcount == 2) {
			strcpy(CURRENTTAG, "start");
		} else {
			startcount++;
		}
	}

	if (!strcmp(el,"end")) {
		if (endcount == 2) {
			strcpy(CURRENTTAG, "end");
		} else {
			endcount++;
		}
	}

	if (!strcmp(el,"port")) {
	   	memset(myPort.protocol,'\0',sizeof(myPort.protocol));
	   	memset(myPort.portnumber,'\0',sizeof(myPort.portnumber));
		strncpy(myPort.protocol,attr[1],min(sizeof(myPort.protocol),strlen(attr[1])));
		if (attr[2]) {
			strncpy(myPort.portnumber,attr[3],min(sizeof(myPort.portnumber),strlen(attr[3])));
		} else {
			strcpy(myPort.portnumber,"0");
		}
		strcpy(CURRENTTAG, "port");
	}

	if (!strcmp(el,"service")) {
	   	memset(myPort.servicename,'\0',sizeof(myPort.servicename));
		strncpy(myPort.servicename,attr[1],min(sizeof(myPort.servicename),strlen(attr[1])));
		strcpy(CURRENTTAG, "servicename");
	}

	if (!strcmp(el,"severity")) {
		strcpy(CURRENTTAG, "severity");
	}

	if (!strcmp(el,"id")) {
		strcpy(CURRENTTAG, "id");
	}

	if (!strcmp(el,"data")) {
	   	memset(myPort.portdata,'\0',sizeof(myPort.portdata));
		strcpy(CURRENTTAG, "data");
	}
}/* End of start_hndl */


void insertHeader() {
	char dbQuery[9000];

	snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO reportheader (scanid,ip,hostname,scanstart,scanend) VALUES (%d,'%s','%s','%s','%s')",myBaseData.myJobNumber, myBaseData.ip, myBaseData.hostname, myBaseData.startdate, myBaseData.enddate);
	logEntry(MYLOG_DEBUG, dbQuery);
	reportid = myDbSelect_NeedRecordId(dbQuery, "reportheader_reportid_seq");
}

void end_hndl(void *data, const char *el) {
	char dbQuery[9000], myPortData[8300];

	if (!strcmp(el,"data")) {
		PQescapeString (myPortData, myPort.portdata, strlen(myPort.portdata));
		snprintf(dbQuery, sizeof(dbQuery), "INSERT INTO reportresults (scanid,reportid,portnumber,protocol,servicename,severity,data,pluginid) VALUES (%d,%d,'%s','%s','%s','%s','%s','%s')",myBaseData.myJobNumber, reportid,myPort.portnumber,myPort.protocol,myPort.servicename,myPort.severity,myPortData, myPort.id);
		logEntry(MYLOG_DEBUG, dbQuery);
		myDbSelect(dbQuery);
	}
}/* End of end_hndl */

void char_hndl(void *data, const char *txt, int txtlen) {
	if (!strcmp(CURRENTTAG,"start")) {
		strncpy(myBaseData.startdate,txt,min(sizeof(myBaseData.startdate),txtlen));
		strcpy(CURRENTTAG, "notstart");
	}
	if (!strcmp(CURRENTTAG,"end")) {
		strncpy(myBaseData.enddate,txt,min(sizeof(myBaseData.enddate),txtlen));
		strcpy(CURRENTTAG, "notend");
		// now the header junk is complete.  insert it.
		insertHeader();
	}
	if (!strcmp(CURRENTTAG,"severity")) {
	  	memset(myPort.severity,'\0',sizeof(myPort.severity));
		strncpy(myPort.severity,txt,min(sizeof(myPort.severity),txtlen));
		strcpy(CURRENTTAG, "notseverity");
	}
	if (!strcmp(CURRENTTAG,"id")) {
	   	memset(myPort.id,'\0',sizeof(myPort.id));
		strncpy(myPort.id,txt,min(sizeof(myPort.id),txtlen));
		strcpy(CURRENTTAG, "notid");
	}

	// yes this has buffer overrun potential.  Dr. Brando will operate later.
	// Ok, the doc is in!  Basically, the problem here lies in the data being on multiple
	// lines.  We need to concatenate all the strings we receive under the data tag to
	// one string for DB insert.  Now, bounds checking gets slightly more complicated because
	// we have multiple strncat calls into one 8K data block.  So we just back down our maximum
	// size by the amount that has been copied thus far.
	if (!strcmp(CURRENTTAG,"data")) {
		strncat(myPort.portdata,txt,min(sizeof(myPort.portdata)-currentLength,txtlen));
		currentLength+=txtlen;
	}
}/* End char_hndl */



void join_threads(void) {
	logEntry(MYLOG_DEBUG,"signaling threads...\n");

	control_deactivate(&wq.control);
	pthread_cond_broadcast(&wq.control.cond);
	while (numthreads) {
        sleep(1);  // wait until all threads exit...
	}
	control_deactivate(&cq.control);
}


/* why do we need this?  if we have bad problems with nessus, we want
   to join that thread and go on with life.  At some point... we will
   probably have a thread manager that will simply watch the cleanup
   queue (much like join_threads() does and join_threads() will simply
   add all threads to the cleanup queue) and join as needed.
*/
/*
	Now this is more like it.  We have a cleanup thread that just watches
	the cleanup queue.
*/
void *cleanupthread(void *myarg) {
	cnode *curnode;

	pthread_mutex_lock(&cq.control.mutex);
	while (cq.control.active) {
		while (cq.cleanup.head==NULL) {
			pthread_cond_wait(&cq.control.cond,&cq.control.mutex);
		}

        if(cq.control.active) {
            curnode = (cnode *) queue_get(&cq.cleanup);
            pthread_mutex_unlock(&cq.control.mutex);
            //pthread_join(curnode->tid,NULL);
            pthread_cancel(curnode->tid);
            free(curnode);
			numthreads--;
            logEntry(MYLOG_DEBUG,"joined one thread");
            pthread_mutex_lock(&cq.control.mutex);
        }
    }
	logEntry(MYLOG_DEBUG,"cleanup thread shutting down...");
    pthread_join(cleanupThreadID, NULL);
	return NULL;
}

int create_threads(int serverID) {
	int x,z;
	cnode *curnode;
 	PGresult *myResult;
	char dbQuery[80];


    pthread_mutex_init(&xmlParsingMutex, NULL);
    pthread_mutex_init(&nessusInstantiationMutex, NULL);
	// Query out all of the scanning servers and set up threads to know who is who.
	if (serverID) {
		snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM scanservers WHERE active=1 AND serverid = %d",serverID);
	} else {
		snprintf(dbQuery, sizeof(dbQuery), "SELECT * FROM scanservers WHERE active=1 AND serverid > 0");
    }
	logEntry(MYLOG_DEBUG, dbQuery);
	myResult = myDbSelect(dbQuery);

	for (z=0; z < PQntuples(myResult); z++) {
		for (x=0; x<atoi(PQgetvalue(myResult, z, 5)); x++) {
			curnode=malloc(sizeof(cnode));

			if (!curnode)
				return 1;

			curnode->threadnum=threadIdSeed;
			threadIdSeed++;
			curnode->scanningServerID=atoi(PQgetvalue(myResult, z, 0));
            memset(curnode->scanningServerAddy,'\0',sizeof(curnode->scanningServerAddy));
            memset(curnode->scanningServerUsername,'\0',sizeof(curnode->scanningServerUsername));
            memset(curnode->scanningServerPassword,'\0',sizeof(curnode->scanningServerPassword));
			strncpy(curnode->scanningServerAddy, PQgetvalue(myResult, z, 1), min(sizeof(curnode->scanningServerAddy)-1,strlen(PQgetvalue(myResult, z, 1))));
			strncpy(curnode->scanningServerUsername, PQgetvalue(myResult, z, 2), min(sizeof(curnode->scanningServerUsername)-1,strlen(PQgetvalue(myResult, z, 2))));
			strncpy(curnode->scanningServerPassword, PQgetvalue(myResult, z, 3), min(sizeof(curnode->scanningServerPassword)-1,strlen(PQgetvalue(myResult, z, 3))));
			curnode->overflow=atoi(PQgetvalue(myResult, z, 4));
	/*
	what I am trying to do here is allow the thread to be a little smarter than
	it previously has been.  I want the thread to know if the server it will be using
	(server id) is an overflow server.  Meaning, if 100 jobs are in the work list,
	other servers will help get through them by processing some nessus jobs.  However,
	they should search the job listing for items specifically for them before selecting an
	overflow job.  These items will be database configured.  I may just need to rewrite this
	whole routine and move my looping control a bit.  I will need to query the database and
	get the scanning servers and number of threads for each one, create the threads, and let
	them nap until its showtime.
	*/
			pthread_mutex_lock(&tq.control.mutex);
			tqueue_put(&tq.thread,(node *) curnode);
			pthread_mutex_unlock(&tq.control.mutex);
			if (pthread_create(&curnode->tid, NULL, scanthread, (void *) curnode))
				return 1;

			logEntry(MYLOG_DEBUG,"created thread");
			numthreads++;
		}
	}
	PQclear(myResult);
	return 0;
}
