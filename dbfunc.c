/******************************************************************
 * ( $Id: dbfunc.c,v 1.14 2003/10/13 15:46:32 brw Exp $ )
 * dbfunc.c
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *      Just a mini abstraction layer to handle my database calls.
 *
 * $Log: dbfunc.c,v $
 * Revision 1.14  2003/10/13 15:46:32  brw
 * fixed some memory leaks
 *
 * Revision 1.13  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 * 
 ******************************************************************/

#include "dbfunc.h"

/* Maybe a global variable or two */
PGconn *myDbHandler;
char DB_HOST[80] = "", DB_USER[80], DB_PASS[80], DB_DATABASE[80], DB_PORT[6];
pthread_mutex_t databaseMutex;
extern short LOG_TO_DATABASE;

/* Abstraction functions */

/* myDbConnect()
 *
 * Basically I just want to handle error stuff for the connect here
 * and select the database.
 */
int myDbConnect() {
    char    connectionString[240];

	/* make connection to the database */
    // build connection string first
    if (strlen(DB_HOST) > 6)
		snprintf(connectionString, sizeof(connectionString), "host=%s port=%s user=%s password=%s dbname=%s", DB_HOST, DB_PORT, DB_USER, DB_PASS, DB_DATABASE);
    else
		snprintf(connectionString, sizeof(connectionString), "user=%s password=%s dbname=%s", DB_USER, DB_PASS, DB_DATABASE);


	myDbHandler = PQconnectdb(connectionString);
	if(PQstatus(myDbHandler) == CONNECTION_BAD) {
		fprintf(stderr,"PQconnectdb() Failed!  %s\n", PQerrorMessage(myDbHandler));
		fprintf(stderr,"Connection String: %s\n", connectionString);
		return(1);
	}

    return(0);
}

/* myDbSelect()
 *
 * Just running a select and returning a resultset if successful,
 * NULL if failure.
 *
 * MONKEYWRENCH!  Need to escape all '.  Also need to re allocate
 * new memory for the new query.
 */
PGresult *myDbSelect(char *query) {
    PGresult    *res;
    char        errormessage[100];

    pthread_mutex_lock(&databaseMutex);

	res = PQexec(myDbHandler,query);

	if((PQresultStatus(res) != PGRES_TUPLES_OK) && (PQresultStatus(res) != PGRES_COMMAND_OK)) {
        snprintf(errormessage, sizeof(errormessage), "PQexec Failed. Code: %d",PQresultStatus(res));
		pthread_mutex_unlock(&databaseMutex);
		logEntry(MYLOG_ERROR,errormessage);
		logEntry(MYLOG_ERROR,PQerrorMessage(myDbHandler));
        LOG_TO_DATABASE=0;
		return (NULL);
	}
	/* store the result from our query */
	pthread_mutex_unlock(&databaseMutex);
	return(res);
}

int myDbSelect_NeedRecordId(const char *query, const char *sequence) {
    PGresult    *res;
    char        idquery[100];
    char        errormessage[100];

    pthread_mutex_lock(&databaseMutex);

	res = PQexec(myDbHandler,query);

	if((PQresultStatus(res) != PGRES_TUPLES_OK) && (PQresultStatus(res) != PGRES_COMMAND_OK)) {
        snprintf(errormessage, sizeof(errormessage), "PQexec Failed. Code: %d %s",PQresultStatus(res),PQerrorMessage(myDbHandler));
		pthread_mutex_unlock(&databaseMutex);
		logEntry(MYLOG_ERROR,errormessage);
		return (0);
	}

    /* now get the ID to return */
    snprintf(idquery, sizeof(idquery), "SELECT currval('%s')", sequence);
    res = PQexec(myDbHandler,idquery);

	pthread_mutex_unlock(&databaseMutex);
	return(atoi(PQgetvalue(res, 0, 0)));
}

/* myDbCleanup
 *
 * Free results and such from memory.
 */
void myDbCleanup(PGresult *res) {
	PQclear(res);
}

/* myDbClose
 *
 * the ender.  close it all
 */
void myDbClose() {
	PQfinish(myDbHandler);
}
