// $Id$

#include "Timer.hh"
#include "Scheduler.hh"

namespace openmsx {

// Force template instantiation
template class Timer<12500, 0x40>;
template class Timer< 3125, 0x20>;


template<int freq, byte flag>
Timer<freq, flag>::Timer(TimerCallback *cb_)
	: count(256), counting(false), cb(cb_),
	  scheduler(Scheduler::instance())
{
}

template<int freq, byte flag>
Timer<freq, flag>::~Timer()
{
	unschedule();
}

template<int freq, byte flag>
void Timer<freq, flag>::setValue(byte value)
{
	count = 256 - value;
}

template<int freq, byte flag>
void Timer<freq, flag>::setStart(bool start, const EmuTime &time)
{
	if (start != counting) {
		counting = start;
		if (start) {
			schedule(time);
		} else {
			unschedule();
		}
	}
}

template<int freq, byte flag>
void Timer<freq, flag>::schedule(const EmuTime &time)
{
	EmuTimeFreq<freq> now(time);
	scheduler.setSyncPoint(now + count, this);
}

template<int freq, byte flag>
void Timer<freq, flag>::unschedule()
{
	scheduler.removeSyncPoint(this);
}

template<int freq, byte flag>
void Timer<freq, flag>::executeUntil(const EmuTime &time, int userData) throw()
{
	cb->callback(flag);
	schedule(time);
}

template<int freq, byte flag>
const string& Timer<freq, flag>::schedName() const
{
	static const string name("Timer");
	return name;
}

} // namespace openmsx
