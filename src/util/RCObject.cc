/*
 *  RCObject.cc
 *  LampiProject
 *
 *  Created by Rob Aulwes on Mon Mar 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "RCObject.h"

void RCObject::retain() {cnt_m++;}

void RCObject::release()
{
	if ( 0 == --cnt_m )
		delete this;
}
	
