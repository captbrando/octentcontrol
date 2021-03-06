/******************************************************************
 * control.h
 * Copyright 2002 Branden R. Williams, Elliptix, LLC.
 * Author: Branden R. Williams
 * Details:
 *		My data control stuff.
 ******************************************************************/

#ifndef CONTROL_H
#define CONTROL_H
#endif

#include <pthread.h>
typedef struct data_control {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int active;
} data_control;

int control_init(data_control *mycontrol);
int control_destroy(data_control *mycontrol);
int control_activate(data_control *mycontrol);
int control_deactivate(data_control *mycontrol);
