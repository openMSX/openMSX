// $Id$

#ifndef CLOCKPIN_HH
#define CLOCKPIN_HH

#include "EmuTime.hh"
#include "Schedulable.hh"

namespace openmsx {

class Scheduler;
class ClockPin;

class ClockPinListener
{
public:
	virtual void signal(ClockPin& pin, const EmuTime& time) = 0;
	virtual void signalPosEdge(ClockPin& pin, const EmuTime& time) = 0;
protected:
	virtual ~ClockPinListener() {}
};

class ClockPin : private Schedulable
{
public:
	ClockPin(Scheduler& scheduler, ClockPinListener* listener = NULL);
	virtual ~ClockPin();

	// input side
	void setState(bool status, const EmuTime& time);
	void setPeriodicState(const EmuDuration& total,
		const EmuDuration& hi, const EmuTime& time);

	// output side
	bool getState(const EmuTime& time) const;
	bool isPeriodic() const;
	const EmuDuration& getTotalDuration() const;
	const EmuDuration& getHighDuration() const;
	int getTicksBetween(const EmuTime& begin,
			    const EmuTime& end) const;

	// control
	void generateEdgeSignals(bool wanted, const EmuTime& time);

private:
	void unschedule();
	void schedule(const EmuTime& time);
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	ClockPinListener* listener;

	EmuDuration totalDur;
	EmuDuration hiDur;
	EmuTime referenceTime;

	bool periodic;
	bool status;
	bool signalEdge;
};

} // namespace openmsx

#endif
