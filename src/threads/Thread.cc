/*
 *  Thread.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "Thread.h"
#include <internal/Private.h>
#include "internal/malloc.h"

#ifdef USE_CT
#include <pthread.h>

#define thread_t				pthread_t
#define thread_create			pthread_create
#define thread_detach			pthread_detach

#else

#define thread_t				int			
#define thread_create(a,b,c,d)		
#define thread_detach(X)		

#endif

#define tInfo		((mpi_thread_info_t *)tinfo_m)

typedef struct
{
	Runnable	*target;
	runImpl_t	run;
	void		*runArg;
} thread_arg_t;

typedef struct
{
	thread_t	tid;
} mpi_thread_info_t;


#ifdef USE_CT

static void *_exec_thread(void *arg)
{
	Thread		*targ = (Thread*)arg;
	Runnable	*target = targ->target();
	runImpl_t	run = targ->runMethod();
	
	(target->*run)(targ->argument());
	
	if ( true == targ->shouldAutoDelete() )
		delete targ;
		
	return NULL;
}

#endif

void Thread::createThread(Runnable *target, runImpl_t tmethod, void *targ)
{
	Thread		*thrd;
	
	thrd = new Thread(target, tmethod, targ);
	thrd->setShouldAutoDelete(true);
}


Thread::Thread(Runnable *target, runImpl_t tmethod, void *targ) : 
		shouldAutoDelete_m(false),
		target_m(target), 
		runMethod_m(tmethod),
		arg_m(targ),
		isRunning_m(false)
{
	init();
	run();
}


void Thread::init()
{
	// need to throw exception on error
    tinfo_m = (mpi_thread_info_t *)ulm_malloc(sizeof(mpi_thread_info_t));
}


void Thread::run()
{	
	if ( true == isRunning_m )
		return;
		
	isRunning_m = true;
	thread_create(&(tInfo->tid), NULL, _exec_thread, this);
	thread_detach(tInfo->tid);
}


Runnable *Thread::target() {return target_m;}
void Thread::setTarget(Runnable *target) {target_m = target;}
	
void *Thread::argument() {return arg_m;}
	
runImpl_t Thread::runMethod() {return runMethod_m;}
