// $Id$

#ifndef __TIMER_HH__
#define __TIMER_HH__

#include "Schedulable.hh"
#include "openmsx.hh"


namespace openmsx {

class Scheduler;

class TimerCallback
{
public:
	virtual void callback(byte value) throw() = 0;
};

template<int freq, byte flag>
class Timer : private Schedulable
{
public:
	Timer(TimerCallback *cb);
	virtual ~Timer();
	void setValue(byte value);
	void setStart(bool start, const EmuTime &time);

private:
	virtual void executeUntil(const EmuTime &time, int userData) throw();
	void schedule(const EmuTime &time);
	void unschedule();

	int count;
	bool counting;
	TimerCallback *cb;
	Scheduler *scheduler;
};

} // namespace openmsx

#endif
