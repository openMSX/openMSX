// $Id$

#ifndef __TIMER_HH__
#define __TIMER_HH__

namespace openmsx {

class Timer
{
public:
	static unsigned long long getTime();
	static void sleep(unsigned long long us);
};

} // namespace openmsx

#endif
