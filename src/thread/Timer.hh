// $Id$

#ifndef __TIMER_HH__
#define __TIMER_HH__

namespace openmsx {

class Timer
{
public:
	static unsigned getTime();
	static void sleep(unsigned us);
};

} // namespace openmsx

#endif
