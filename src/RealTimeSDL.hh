// $Id$

#ifndef __MSXREALTIMESDL_HH__
#define __MSXREALTIMESDL_HH__

#include "RealTime.hh"

namespace openmsx {

class RealTimeSDL : public RealTime
{
public:
	RealTimeSDL();

protected:
	virtual unsigned getRealTime();
	virtual void doSleep(unsigned ms);
	virtual void reset();
};

} // namespace openmsx

#endif
