// $Id$

// This class implements the Intel 8254 chip (and 8253)
//
// * Only the 8254 is emulated, no surrounding hardware.
//   Use the class I8254Interface to do that.

#ifndef I8254_HH
#define I8254_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class EmuTime;
class Scheduler;
class Counter;
class ClockPin;
class ClockPinListener;

class I8254 : private noncopyable
{
public:
	I8254(Scheduler& scheduler, ClockPinListener* output0,
	      ClockPinListener* output1, ClockPinListener* output2,
	      const EmuTime& time);
	~I8254();

	void reset(const EmuTime& time);
	byte readIO(word port, const EmuTime& time);
	byte peekIO(word port, const EmuTime& time) const;
	void writeIO(word port, byte value, const EmuTime& time);

	void setGate(unsigned counter, bool status, const EmuTime& time);
	ClockPin& getClockPin(unsigned cntr);
	ClockPin& getOutputPin(unsigned cntr);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void readBackHelper(byte value, unsigned cntr, const EmuTime& time);

	std::auto_ptr<Counter> counter[3];
};

} // namespace openmsx

#endif
