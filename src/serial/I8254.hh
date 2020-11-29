// This class implements the Intel 8254 chip (and 8253)
//
// * Only the 8254 is emulated, no surrounding hardware.
//   Use the class I8254Interface to do that.

#ifndef I8254_HH
#define I8254_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class Scheduler;
class Counter;
class ClockPin;
class ClockPinListener;

class I8254
{
public:
	I8254(Scheduler& scheduler, ClockPinListener* output0,
	      ClockPinListener* output1, ClockPinListener* output2,
	      EmuTime::param time);
	~I8254();

	void reset(EmuTime::param time);
	[[nodiscard]] byte readIO(word port, EmuTime::param time);
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const;
	void writeIO(word port, byte value, EmuTime::param time);

	void setGate(unsigned cntr, bool status, EmuTime::param time);
	[[nodiscard]] ClockPin& getClockPin(unsigned cntr);
	[[nodiscard]] ClockPin& getOutputPin(unsigned cntr);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void readBackHelper(byte value, unsigned cntr, EmuTime::param time);

private:
	std::unique_ptr<Counter> counter[3];
};

} // namespace openmsx

#endif
