// $Id$

#include "EmuTimer.hh"
#include "Scheduler.hh"
#include "Clock.hh"

using std::string;

namespace openmsx {

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::EmuTimer(EmuTimerCallback* cb_)
	: count(256), counting(false), cb(cb_),
	  scheduler(Scheduler::instance())
{
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::~EmuTimer()
{
	unschedule();
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::setValue(byte value)
{
	count = 256 - value;
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::setStart(
	bool start, const EmuTime& time)
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

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::schedule(const EmuTime& time)
{
	Clock<FREQ_NOM, FREQ_DENOM> now(time);
	now += count;
	scheduler.setSyncPoint(now.getTime(), *this);
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::unschedule()
{
	scheduler.removeSyncPoint(*this);
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::executeUntil(
	const EmuTime& time, int /*userData*/)
{
	cb->callback(FLAG);
	schedule(time);
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
const string& EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM>::schedName() const
{
	static const string name("EmuTimer");
	return name;
}


// Force template instantiation
template class EmuTimer<0x40,  3579545, 72 *  4    >; // EmuTimerOPL3_1
template class EmuTimer<0x20,  3579545, 72 *  4 * 4>; // EmuTimerOPL3_2
template class EmuTimer<0x40, 33868800, 72 * 38    >; // EmuTimerOPL4_1
template class EmuTimer<0x20, 33868800, 72 * 38 * 4>; // EmuTimerOPL4_2

} // namespace openmsx
