// $Id$

#include <cassert>
#include "ClockPin.hh"
#include "Scheduler.hh"


ClockPin::ClockPin(ClockPinListener* listener_)
	: listener(listener_), periodic(false), status(false),
	  signalEdge(false)
{
	scheduler = Scheduler::instance();
}

ClockPin::~ClockPin()
{
	unschedule();
}


void ClockPin::setState(bool newStatus, const EmuTime& time)
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

void ClockPin::setPeriodicState(const EmuDuration& total,
	const EmuDuration& hi, const EmuTime& time)
{
	periodic = true;
	referenceTime = time;
	totalDur = total;
	hiDur = hi;
	
	if (listener) {
		if (signalEdge) {
			executeUntilEmuTime(time, 0);
		}
		listener->signal(*this, time);
	}
}


bool ClockPin::getState(const EmuTime& time) const
{
	if (!periodic) {
		return status;
	} else {
		return ((time - referenceTime) % totalDur) < hiDur;
	}
}

bool ClockPin::isPeriodic() const
{
	return periodic;
}

const EmuDuration& ClockPin::getTotalDuration() const 
{
	if (periodic) {
		return totalDur;
	} else {
		return EmuDuration::infinity;
	}
}

const EmuDuration& ClockPin::getHighDuration() const
{
	if (periodic) {
		return  hiDur;
	} else {
		return EmuDuration::infinity;
	}
}

int ClockPin::getTicksBetween(const EmuTime& begin, const EmuTime& end) const
{
	assert(begin <= end);
	if (!periodic) {
		return 0;
	}
	if (totalDur > EmuDuration::zero) {
		int a = (begin < referenceTime) ? 
		        0 : 
		        (begin - referenceTime) / totalDur;
		int b = (end   - referenceTime) / totalDur;
		return b - a;
	} else {
		return 0;
	}
}


void ClockPin::generateEdgeSignals(bool wanted, const EmuTime& time)
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
	scheduler->removeSyncPoint(this);
}

void ClockPin::schedule(const EmuTime& time)
{
	scheduler->setSyncPoint(time, this);
}

void ClockPin::executeUntilEmuTime(const EmuTime& time, int userdata)
{
	assert(signalEdge && periodic);
	listener->signalPosEdge(*this, time);
	if (totalDur > EmuDuration::zero) {
		schedule(time + totalDur);
	}
}

const string& ClockPin::schedName() const
{
	static const string name("ClockPin");
	return name;
}
