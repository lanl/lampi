/*
 * Copyright 2002-2004. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Thread.h"
#include <internal/Private.h>
#include "internal/malloc.h"

#if ENABLE_CT
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


#if ENABLE_CT

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
