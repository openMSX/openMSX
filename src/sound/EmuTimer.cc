// $Id$

#include "EmuTimer.hh"
#include "Clock.hh"
#include "serialize.hh"

namespace openmsx {

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::EmuTimer(
		Scheduler& scheduler, EmuTimerCallback& cb_)
	: Schedulable(scheduler), cb(cb_), count(MAXVAL), counting(false)
{
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::setValue(int value)
{
	count = MAXVAL - value;
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::setStart(
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

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::schedule(const EmuTime& time)
{
	Clock<FREQ_NOM, FREQ_DENOM> now(time);
	now += count;
	setSyncPoint(now.getTime());
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::unschedule()
{
	removeSyncPoint();
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::executeUntil(
	const EmuTime& time, int /*userData*/)
{
	cb.callback(FLAG);
	schedule(time);
}

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
const std::string& EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::schedName() const
{
	static const std::string name("EmuTimer");
	return name;
}


template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
template<typename Archive>
void EmuTimer<FLAG, FREQ_NOM, FREQ_DENOM, MAXVAL>::serialize(
	Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("count", count);
	ar.serialize("counting", counting);
}

// Force template instantiation
template class EmuTimer<0x40,  3579545, 64 * 2     , 1024>; // EmuTimerOPM_1
template class EmuTimer<0x20,  3579545, 64 * 2 * 16, 256 >; // EmuTimerOPM_2
template class EmuTimer<0x40,  3579545, 72 *  4    , 256 >; // EmuTimerOPL3_1
template class EmuTimer<0x20,  3579545, 72 *  4 * 4, 256 >; // EmuTimerOPL3_2
template class EmuTimer<0x40, 33868800, 72 * 38    , 256 >; // EmuTimerOPL4_1
template class EmuTimer<0x20, 33868800, 72 * 38 * 4, 256 >; // EmuTimerOPL4_2

INSTANTIATE_SERIALIZE_METHODS(EmuTimerOPM_1);
INSTANTIATE_SERIALIZE_METHODS(EmuTimerOPM_2);
INSTANTIATE_SERIALIZE_METHODS(EmuTimerOPL3_1);
INSTANTIATE_SERIALIZE_METHODS(EmuTimerOPL3_2);
INSTANTIATE_SERIALIZE_METHODS(EmuTimerOPL4_1);
INSTANTIATE_SERIALIZE_METHODS(EmuTimerOPL4_2);

} // namespace openmsx
