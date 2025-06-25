#include "ClockPin.hh"
#include "narrow.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

ClockPin::ClockPin(Scheduler& scheduler_, ClockPinListener* listener_)
	: Schedulable(scheduler_), listener(listener_)
{
}

void ClockPin::setState(bool newStatus, EmuTime::param time)
{
	periodic = false;
	if (signalEdge) {
		unschedule();
	}
	if (signalEdge && !status && newStatus) {
		// pos edge
		status = newStatus;
		if (listener) {
			listener->signalPosEdge(*this, time);
		}
	} else {
		status = newStatus;
	}
	if (listener) {
		listener->signal(*this, time);
	}
}

void ClockPin::setPeriodicState(EmuDuration total,
	EmuDuration hi, EmuTime::param time)
{
	referenceTime = time;
	totalDur = total;
	hiDur = hi;

	if (listener) {
		if (periodic) {
			unschedule();
		}
		periodic = true;
		if (signalEdge) {
			executeUntil(time);
		}
		listener->signal(*this, time);
	} else {
		periodic = true;
	}
}


bool ClockPin::getState(EmuTime::param time) const
{
	if (!periodic) {
		return status;
	} else {
		return ((time - referenceTime) % totalDur) < hiDur;
	}
}

EmuDuration ClockPin::getTotalDuration() const
{
	assert(periodic);
	return totalDur;
}

EmuDuration ClockPin::getHighDuration() const
{
	assert(periodic);
	return hiDur;
}

unsigned ClockPin::getTicksBetween(EmuTime::param begin, EmuTime::param end) const
{
	assert(begin <= end);
	if (!periodic) {
		return 0;
	}
	if (totalDur > EmuDuration::zero()) {
		auto a = (begin < referenceTime)
		       ? 0
		       : (begin - referenceTime) / totalDur;
		auto b = (end   - referenceTime) / totalDur;
		return b - a;
	} else {
		return 0;
	}
}


void ClockPin::generateEdgeSignals(bool wanted, EmuTime::param time)
{
	if (signalEdge != wanted) {
		signalEdge = wanted;
		if (periodic) {
			if (signalEdge) {
				EmuTime tmp(referenceTime);
				while (tmp < time) {
					tmp += totalDur;
				}
				if (listener) {
					schedule(tmp);
				}
			} else {
				unschedule();
			}
		}
	}
}

void ClockPin::unschedule()
{
	removeSyncPoint();
}

void ClockPin::schedule(EmuTime::param time)
{
	assert(signalEdge && periodic && listener);
	setSyncPoint(time);
}

void ClockPin::executeUntil(EmuTime::param time)
{
	assert(signalEdge && periodic && listener);
	listener->signalPosEdge(*this, time);
	if (signalEdge && (totalDur > EmuDuration::zero())) {
		schedule(time + totalDur);
	}
}


template<typename Archive>
void ClockPin::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("totalDur",      totalDur,
	             "hiDur",         hiDur,
	             "referenceTime", referenceTime,
	             "periodic",      periodic,
	             "status",        status,
	             "signalEdge",    signalEdge);
}
INSTANTIATE_SERIALIZE_METHODS(ClockPin);

} // namespace openmsx
