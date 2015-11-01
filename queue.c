/******************************************************************
 * ( $Id: queue.c,v 1.14 2003/04/02 18:12:47 brw Exp $ )
 * 
 * queue.c
 * Copyright 2002-2003 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *	This is a dumb (thread ignorant) set of queueing functions
 *	that allows for easy management of jobs.  There is a special
 * 	function that has smarts enough to determine WHICH job to
 * 	return.
 *
 * $Log: queue.c,v $
 * Revision 1.14  2003/04/02 18:12:47  brw
 * fixed copyright and added log/id cvs tags
 * 
 *
 ******************************************************************/

#include "mypthreads.h"
extern int numthreads;

void queue_init(queue *myroot) {
	myroot->head=NULL;
	myroot->tail=NULL;
}

void queue_put(queue *myroot,node *mynode) {
	mynode->next=NULL;
	if (myroot->tail!=NULL)
		myroot->tail->next=mynode;
	myroot->tail=mynode;
	if (myroot->head==NULL)
		myroot->head=mynode;
}

void tqueue_put(queue *myroot,node *mynode) {
	mynode->tnext=NULL;
	if (myroot->tail!=NULL)
		myroot->tail->tnext=mynode;
	myroot->tail=mynode;
	if (myroot->head==NULL)
		myroot->head=mynode;
}

/* I had to make this smarter.  I want to search for a node with
   my server ID in it first.  If I find one, return it, otherwise
   search for some overflow if we accept it.  Finally just return
   NULL if we don't find any work at all to do.
*/
node *smart_queue_get(queue *myroot, int serverID, int overflow) {
	node *currnode, *prevnode;
	wnode *mynode;
	int found=0;

	/* first make sure there are still entries in the list...	remove this later...*/
	if (myroot->head==NULL) {
		logEntry(MYLOG_DEBUG,"Ooops, no more entries....");
	}

	/* Now flip through the linked list and find some work that is just for us */
	if (myroot->head!=NULL) {
		currnode=myroot->head;
		while ((currnode != NULL) && (!found)) {
			mynode = (wnode *) currnode;
			if (mynode->scanningServerID == serverID) {
				found=1;
			} else {
				prevnode = currnode;
	  			currnode = currnode->next;
			}
		}
  	}

	/* if we did not find any just for us and we take overflow work, lets just
	   see if we can find any general work to take. */
	if (!found && overflow==1) {
		prevnode = NULL;   // DONT FORGET TO CLEAR POINTERS DANGIT!
		currnode=myroot->head;
		while ((currnode != NULL) && (!found)) {
			mynode = (wnode *) currnode;
			if (mynode->scanningServerID == 0) {
				found=1;
			} else {
				prevnode = currnode;
	  			currnode = currnode->next;
			}
		}
	}

	if (found) {
		if (currnode == myroot->head) {
			myroot->head=myroot->head->next;
			if (currnode == myroot->tail) {
				myroot->tail = NULL;
			}
		} else {
			prevnode->next = currnode->next;
			if (currnode == myroot->tail) {
				myroot->tail = prevnode;
			}
		}
	} else {
		return NULL;
	}

	return currnode;
}

void killThreads(queue *myroot, int serverID) {
	node *currnode, *prevnode;
	cnode *mynode;
    int x=0;

	/* first make sure there are still entries in the list...	remove this later...*/
	if (myroot->head==NULL) {
		logEntry(MYLOG_DEBUG,"What?  Someone ordered me to kill threads and there are no more....");
	}

	pthread_mutex_lock(&cq.control.mutex);
	/* Now flip through the linked list and find threads that should die */
	if (myroot->head!=NULL) {
		currnode=myroot->head;
		while (currnode != NULL) {
			mynode = (cnode *) currnode;
            x++;
			if (mynode->scanningServerID == serverID) {
				queue_put(&cq.cleanup,(node *) mynode);
                // To Prevent Deadlock
				pthread_mutex_unlock(&cq.control.mutex);
                remove_node(&tq.thread, mynode->threadnum);
				pthread_mutex_lock(&cq.control.mutex);
				logEntry(MYLOG_DEBUG,"Added thread to kill queue.");
			}
			prevnode = currnode;
  			currnode = currnode->tnext;
		}
  	}

	pthread_mutex_unlock(&cq.control.mutex);
	pthread_cond_signal(&cq.control.cond);
	pthread_cond_signal(&wq.control.cond);
}

void remove_node(queue *myroot, int threadnum) {
	node *currnode, *prevnode;
	cnode *mynode;
    int found=0;

	pthread_mutex_lock(&tq.control.mutex);

	/* Now flip through the linked list and find our thread to be removed */
	if (myroot->head!=NULL) {
		currnode=myroot->head;
		while ((currnode != NULL) && (!found)) {
			mynode = (cnode *) currnode;
			if (mynode->threadnum == threadnum) {
                found=1;
			} else {
				prevnode = currnode;
  				currnode = currnode->tnext;
            }
		}
  	}

	if (found) {
		if (currnode == myroot->head) {
			myroot->head=myroot->head->tnext;
			if (currnode == myroot->tail) {
				myroot->tail = NULL;
			}
		} else {
			prevnode->tnext = currnode->tnext;
			if (currnode == myroot->tail) {
				myroot->tail = prevnode;
			}
		}
    }

	pthread_mutex_unlock(&tq.control.mutex);

	return;
}



node *queue_get(queue *myroot) {
	//pop off the top
	node *mynode;
	mynode=myroot->head;

	// First check to see if this is the only element in the list.
	if (myroot->head==myroot->tail)
		myroot->tail=NULL;

	if (myroot->head!=NULL)
		myroot->head=myroot->head->next;
	return mynode;
}

void queue_print(queue *myroot) {
	node *currnode;
	wnode *mynode;
	int found=0;

	if (myroot->head!=NULL) {
		currnode=myroot->head;
		while ((currnode != NULL) && (!found)) {
			mynode = (wnode *) currnode;
			fprintf(stderr,"%s Only for host %d.  Job number %d\n", mynode->networknode, mynode->scanningServerID, mynode->jobnum);
   			currnode = currnode->next;
		}
  	}
}
