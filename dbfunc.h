/******************************************************************
 * ( $Id: dbfunc.h,v 1.8 2003/04/02 18:12:47 brw Exp $ )
 * dbfunc.h
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *      Just a mini abstraction layer to handle my database calls.
 *
 * $Log: dbfunc.h,v $
 * Revision 1.8  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#ifndef DBFUNC_H
#define DBFUNC_H
#endif

#ifndef LOG_H
#include "log.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <pthread.h>

/* Abstraction functions */
int myDbConnect();
PGresult *myDbSelect(char *query);
int myDbSelect_NeedRecordId(const char *query, const char *sequence);
void myDbCleanup(PGresult *res);
void myDbClose();
