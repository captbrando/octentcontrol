/******************************************************************
 * ( $Id: queue.h,v 1.8 2003/04/02 18:12:47 brw Exp $ )
 * 
 * queue.h
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *	This is the header file for my queueing stuff.  Basically
 *	we use the queue to handle our threads.  It looks for new
 *	jobs and handles cleanup of old threads.
 *
 * $Log: queue.h,v $
 * Revision 1.8  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 *
 *
 ******************************************************************/

#ifndef QUEUE_H
#define QUEUE_H
#endif

#ifndef MYPTHREADS_H
#include "mypthreads.h"
#endif

#include <libpq-fe.h>

typedef struct node {
  struct node *next;
  struct node *tnext;
} node;
typedef struct queue {
  node *head, *tail;
} queue;
void queue_init(queue *myroot);
void queue_put(queue *myroot, node *mynode);
void tqueue_put(queue *myroot, node *mynode);
node *smart_queue_get(queue *myroot, int serverID, int overflow);
node *queue_get(queue *myroot);
void queue_print(queue *myroot);
void killThreads(queue *myroot, int serverID);
void remove_node(queue *myroot, int threadnum);
