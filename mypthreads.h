/******************************************************************
 * ( $Id: mypthreads.h,v 1.11 2003/04/02 18:12:47 brw Exp $ )
 *
 * mypthreads.h
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		Header file for some items related to the threads.
 *
 * $Log: mypthreads.h,v $
 * Revision 1.11  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#ifndef MYPTHREADS_H
#define MYPTHREADS_H
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <libpq-fe.h>
#include <string.h>
#include <unistd.h>

#ifndef CONTROL_H
#include "control.h"
#endif

#ifndef XMLPARSE_H
#include "xmlparse.h"
#endif

#ifndef QUEUE_H
#include "queue.h"
#endif

#ifndef DBFUNC_H
#include "dbfunc.h"
#endif

#ifndef BRWHELPERS_H
#include "brwhelpers.h"
#endif

/* the work_queue holds tasks for the various threads to complete. */
struct work_queue {
	data_control control;
	queue work;
} wq;

/* All of the data to be processed.  This will mimic the pertinent information from
   the scanqueue table. */
typedef struct work_node {
	struct			node *next;
	unsigned short	jobnum;
    int 			scanningServerID;
	char			networknode[75]; /* Just an IP address really.  Each work node will have its own ip */
	char			*configfile;  /* the config file we are going to use. */
} wnode;

/* the cleanup queue holds stopped threads.  Before a thread
   terminates, it adds itself to this list.  Since the main thread is
   waiting for changes in this list, it will then wake up and clean up
   the newly terminated thread. */
struct cleanup_queue {
	data_control control;
	queue cleanup;
} cq;

/* I added a thread number (for debugging/instructional purposes) and
   a thread id to the cleanup node.  The cleanup node gets passed to
   the new thread on startup, and just before the thread stops, it
   attaches the cleanup node to the cleanup queue.  The main thread
   monitors the cleanup queue and is the one that performs the
   necessary cleanup. */
typedef struct cleanup_node {
	struct node *next;
	struct node *tnext;
	int threadnum;
    int scanningServerID;
    char scanningServerAddy[35];
    char scanningServerUsername[35];
    char scanningServerPassword[35];
    int overflow;
	pthread_t tid;
} cnode;

// Gotta create my own thread queue.
struct thread_queue {
	data_control control;
	queue thread;
} tq;

void start_hndl(void *data, const char *el, const char **attr);
void end_hndl(void *data, const char *el);
void insertHeader();
void char_hndl(void *data, const char *txt, int txtlen);
void join_threads(void);
int create_threads(int serverID);
void *cleanupthread(void *myarg);
