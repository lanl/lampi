/*
 *  Runnable.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */


#ifndef _RUNNABLE_H_
#define _RUNNABLE_H_

#include <internal/MPIIncludes.h>


class Runnable
{
	/*
	 *		Instance Methods
	 */
	protected:
	virtual void run(void *arg) {};
};

typedef void (Runnable::*runImpl_t)(void *) const;

#endif
