#include "EmuTimer.hh"
#include "serialize.hh"
#include <memory>

using std::unique_ptr;

namespace openmsx {

unique_ptr<EmuTimer> EmuTimer::createOPM_1(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 1,  3579545, 64 * 2     , 1024);
}

unique_ptr<EmuTimer> EmuTimer::createOPM_2(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 2,  3579545, 64 * 2 * 16, 256);
}

unique_ptr<EmuTimer> EmuTimer::createOPP_2(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 2,  3579545, 64 * 2 * 32, 256);
}

unique_ptr<EmuTimer> EmuTimer::createOPL3_1(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 0x40,  3579545, 72 *  4    , 256);
}

unique_ptr<EmuTimer> EmuTimer::createOPL3_2(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 0x20,  3579545, 72 *  4 * 4, 256);
}

unique_ptr<EmuTimer> EmuTimer::createOPL4_1(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 0x40, 33868800, 72 * 38    , 256);
}

unique_ptr<EmuTimer> EmuTimer::createOPL4_2(
	Scheduler& scheduler, EmuTimerCallback& cb)
{
	return std::make_unique<EmuTimer>(
		scheduler, cb, 0x20, 33868800, 72 * 38 * 4, 256);
}


EmuTimer::EmuTimer(Scheduler& scheduler_, EmuTimerCallback& cb_,
                   byte flag_, unsigned freq_num, unsigned freq_denom,
                   unsigned maxval_)
	: Schedulable(scheduler_), cb(cb_)
	, clock(EmuTime::dummy())
	, maxval(maxval_), count(maxval_)
	, flag(flag_), counting(false)
{
	clock.setFreq(freq_num, freq_denom);
}

void EmuTimer::setValue(int value)
{
	count = maxval - value;
}

void EmuTimer::setStart(bool start, EmuTime::param time)
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

void EmuTimer::schedule(EmuTime::param time)
{
	clock.reset(time);
	clock += count;
	setSyncPoint(clock.getTime());
}

void EmuTimer::unschedule()
{
	removeSyncPoint();
}

void EmuTimer::executeUntil(EmuTime::param time)
{
	cb.callback(flag);
	schedule(time);
}

void EmuTimer::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("count",    count,
	             "counting", counting);
}
INSTANTIATE_SERIALIZE_METHODS(EmuTimer);

} // namespace openmsx
