/*
 *  RCObject.h
 *  LampiProject
 *
 *  Created by Rob Aulwes on Mon Mar 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _RC_OBJECT_
#define _RC_OBJECT_

/*
This class provides a base class for objects that wish to use
reference counting.  If any class is derived from this class, then
the subclass should NEVER be freed using delete directly.  Any instances
of subclasses should always be managed with retain/release.
*/

class RCObject
{
	private:
	int		cnt_m;
	
	public:
	RCObject() : cnt_m(1) {}
	virtual ~RCObject() {}
	
	virtual void retain();
	virtual void release();
	
};

#endif

