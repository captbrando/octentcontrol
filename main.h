/******************************************************************
 * ( $Id: main.h,v 1.8 2003/04/02 18:12:47 brw Exp $ )
 *
 * main.h
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		try to control circular references...
 * 
 * $Log: main.h,v $
 * Revision 1.8  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#ifndef MAIN_H
#define MAIN_H
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <libpq-fe.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef LOG_H
#include "log.h"
#endif

#ifndef DEBUG_H
#include "debug.h"
#endif

#ifndef BRWHELPERS_H
#include "brwhelpers.h"
#endif

#ifndef MYPTHREADS_H
#include "mypthreads.h"
#endif

#ifndef CONTROL_H
#include "control.h"
#endif

#ifndef min
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#endif

void cleanup_structs(void);
