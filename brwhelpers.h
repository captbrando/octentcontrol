/******************************************************************
 * ( $Id: brwhelpers.h,v 1.9 2003/04/02 18:12:47 brw Exp $ )
 * brwhelpers.h
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		Need to thread-enable the queue.
 *
 * $Log: brwhelpers.h,v $
 * Revision 1.9  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#ifndef BRWHELPERS_H
#define BRWHELPERS_H
#endif

#include <signal.h>
#include <string.h>
#include <unistd.h>

#ifndef MYPTHREADS_H
#include "mypthreads.h"
#endif

#ifndef LOG_H
#include "log.h"
#endif

#ifndef MAIN_H
#include "main.h"
#endif

#ifndef min
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#endif

/* splitIpAddress simply takes the 4 octets and breaks them up */
int splitIpAddress(char *ip_ptr, short *first, short *second, short *third, short *fourth);

/* I need to cleanly shut down so I trap SIGTERM */
void sigControl(int sig);

/* some helper functions that are called from main.c */
void populateEntries (int myJobNum, int myServerID);
void recoverFromCrash(void);
void checkForWork(void);
void checkForMessages(void);
void requeueKilledServerWork (int myServerID);
