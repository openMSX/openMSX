// $Id$

#include "EmuTimer.hh"
#include "Scheduler.hh"

using std::string;

namespace openmsx {

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
	Clock<freq> now(time);
	now += count;
	scheduler.setSyncPoint(now.getTime(), this);
}

template<int freq, byte flag>
void EmuTimer<freq, flag>::unschedule()
{
	scheduler.removeSyncPoint(this);
}

template<int freq, byte flag>
void EmuTimer<freq, flag>::executeUntil(const EmuTime& time, int /*userData*/)
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


// Force template instantiation
template class EmuTimer<12429, 0x40>; //  3.579545 MHz / (72 * 4)
template class EmuTimer< 3107, 0x20>; //  3.579545 MHz / (72 * 4 * 4)
template class EmuTimer<12379, 0x40>; // 33.8688   MHz / (72 * 38)
template class EmuTimer< 3095, 0x20>; // 33.8688   MHz / (72 * 38 * 4)

} // namespace openmsx
