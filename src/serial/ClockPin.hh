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
	virtual void signal(ClockPin& pin, EmuTime time) = 0;
	virtual void signalPosEdge(ClockPin& pin, EmuTime time) = 0;

protected:
	~ClockPinListener() = default;
};

class ClockPin final : public Schedulable
{
public:
	explicit ClockPin(Scheduler& scheduler, ClockPinListener* listener = nullptr);

	// input side
	void setState(bool status, EmuTime time);
	void setPeriodicState(EmuDuration total,
	                      EmuDuration hi, EmuTime time);

	// output side
	[[nodiscard]] bool getState(EmuTime time) const;
	[[nodiscard]] bool isPeriodic() const { return periodic; }
	[[nodiscard]] EmuDuration getTotalDuration() const;
	[[nodiscard]] EmuDuration getHighDuration() const;
	[[nodiscard]] unsigned getTicksBetween(EmuTime begin,
	                                       EmuTime end) const;

	// control
	void generateEdgeSignals(bool wanted, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void unschedule();
	void schedule(EmuTime time);
	void executeUntil(EmuTime time) override;

private:
	ClockPinListener* const listener;

	EmuDuration totalDur;
	EmuDuration hiDur;
	EmuTime referenceTime = EmuTime::zero();

	bool periodic = false;
	bool status = false;
	bool signalEdge = false;
};

} // namespace openmsx

#endif
