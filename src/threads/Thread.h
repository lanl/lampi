/*
 *  Thread.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */


#ifndef _THREAD_H_
#define _THREAD_H_

#include <internal/MPIIncludes.h>
#include <threads/Runnable.h>

class Thread
{
	/*
	 *		Instance variables
	 */
	
	private:
	bool		shouldAutoDelete_m;
	
	protected:
	void		*tinfo_m;
	Runnable	*target_m;
	runImpl_t	runMethod_m;
	void		*arg_m;
	bool		isRunning_m;


	/*
	 *		Instance methods
	 */
	 
	protected:
	virtual void init();
	
	public:
	static void createThread(Runnable *target, runImpl_t tmethod, void *targ);
	
	Thread(Runnable *target, runImpl_t tmethod, void *targ);
	/*
		POST:	Creates Thread instance and executes tmethod in
				a separate thread.
	*/
	
	void run();
	
	/*
	 *		Accessor methods
	 */
	 
	 
	Runnable *target() ;
	void setTarget(Runnable *target) ;
	
	void *argument() ;
	
	runImpl_t runMethod() ;
	
	bool shouldAutoDelete() {return shouldAutoDelete_m;}
	void setShouldAutoDelete(bool yn) {shouldAutoDelete_m = yn;}
};

#endif
