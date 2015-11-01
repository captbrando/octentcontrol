/******************************************************************
 * ( $Id: log.h,v 1.4 2003/04/02 18:12:47 brw Exp $ )
 *
 * log.h
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		Abstracting for log functions...
 * 
 * $Log: log.h,v $
 * Revision 1.4  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#ifndef LOG_H
#define LOG_H
#endif

#include <stdio.h>
#include <syslog.h>
#include <string.h>

#ifndef DBFUNC_H
#include "dbfunc.h"
#endif

#ifndef MYLOG_ERROR
#define MYLOG_ERROR 'e'
#endif

#ifndef MYLOG_SCAN
#define MYLOG_SCAN 's'
#endif

#ifndef MYLOG_INFO
#define MYLOG_INFO 'i'
#endif

#ifndef MYLOG_DEBUG
#define MYLOG_DEBUG 'd'
#endif

/* some prototypes */
void logEntry(char logType, char *logAction);
void openLog(void);
void closeLog(void);

/* some globals. */
FILE *myLogFile;
