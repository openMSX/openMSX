// $Id$

#ifndef TIMER_HH
#define TIMER_HH

namespace openmsx {

class Timer
{
public:
	static unsigned long long getTime();
	static void sleep(unsigned long long us);
};

} // namespace openmsx

#endif
