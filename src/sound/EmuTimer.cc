// $Id$

#include "EmuTimer.hh"
#include "Scheduler.hh"

namespace openmsx {

// Force template instantiation
template class EmuTimer<12500, 0x40>;
template class EmuTimer< 3125, 0x20>;


template<int freq, byte flag>
EmuTimer<freq, flag>::EmuTimer(EmuTimerCallback* cb_)
	: count(256), counting(false), cb(cb_),
	  scheduler(Scheduler::instance())
{
}

template<int freq, byte flag>
EmuTimer<freq, flag>::~EmuTimer()
{
	unschedule();
}

template<int freq, byte flag>
void EmuTimer<freq, flag>::setValue(byte value)
{
	count = 256 - value;
}

template<int freq, byte flag>
void EmuTimer<freq, flag>::setStart(bool start, const EmuTime& time)
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
void EmuTimer<freq, flag>::schedule(const EmuTime& time)
{
	EmuTimeFreq<freq> now(time);
	scheduler.setSyncPoint(now + count, this);
}

template<int freq, byte flag>
void EmuTimer<freq, flag>::unschedule()
{
	scheduler.removeSyncPoint(this);
}

template<int freq, byte flag>
void EmuTimer<freq, flag>::executeUntil(const EmuTime& time, int userData)
throw()
{
	cb->callback(flag);
	schedule(time);
}

template<int freq, byte flag>
const string& EmuTimer<freq, flag>::schedName() const
{
	static const string name("EmuTimer");
	return name;
}

} // namespace openmsx
