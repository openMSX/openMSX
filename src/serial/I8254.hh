// This class implements the Intel 8254 chip (and 8253)
//
// * Only the 8254 is emulated, no surrounding hardware.
//   Use the class I8254Interface to do that.

#ifndef I8254_HH
#define I8254_HH

#include "ClockPin.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include <array>

namespace openmsx {

class Scheduler;
class ClockPinListener;

class Counter {
public:
	Counter(Scheduler& scheduler, ClockPinListener* listener,
		EmuTime::param time);
	void reset(EmuTime::param time);
	[[nodiscard]] byte readIO(EmuTime::param time);
	[[nodiscard]] byte peekIO(EmuTime::param time) const;
	void writeIO(byte value, EmuTime::param time);
	void setGateStatus(bool status, EmuTime::param time);
	void writeControlWord(byte value, EmuTime::param time);
	void latchStatus(EmuTime::param time);
	void latchCounter(EmuTime::param time);

	void serialize(Archive auto& ar, unsigned version);

//private:
	enum ByteOrder {LOW, HIGH};

private:
	static constexpr byte WRT_FRMT = 0x30;
	static constexpr byte WF_LATCH = 0x00;
	static constexpr byte WF_LOW   = 0x10;
	static constexpr byte WF_HIGH  = 0x20;
	static constexpr byte WF_BOTH  = 0x30;
	static constexpr byte CNTR_MODE = 0x0E;
	static constexpr byte CNTR_M0   = 0x00;
	static constexpr byte CNTR_M1   = 0x02;
	static constexpr byte CNTR_M2   = 0x04;
	static constexpr byte CNTR_M3   = 0x06;
	static constexpr byte CNTR_M4   = 0x08;
	static constexpr byte CNTR_M5   = 0x0A;
	static constexpr byte CNTR_M2_  = 0x0C;
	static constexpr byte CNTR_M3_  = 0x0E;

	void writeLoad(word value, EmuTime::param time);
	void advance(EmuTime::param time);

private:
	ClockPin clock;
	ClockPin output;
	EmuTime currentTime;
	int counter;
	word latchedCounter, counterLoad;
	byte control, latchedControl;
	bool ltchCtrl, ltchCntr;
	ByteOrder readOrder, writeOrder;
	byte writeLatch;
	bool gate;
	bool active, triggered, counting;

	friend class I8254;
};

class I8254
{
public:
	I8254(Scheduler& scheduler, ClockPinListener* output0,
	      ClockPinListener* output1, ClockPinListener* output2,
	      EmuTime::param time);

	void reset(EmuTime::param time);
	[[nodiscard]] byte readIO(word port, EmuTime::param time);
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const;
	void writeIO(word port, byte value, EmuTime::param time);

	void setGate(unsigned cntr, bool status, EmuTime::param time);
	[[nodiscard]] ClockPin& getClockPin(unsigned cntr);
	[[nodiscard]] ClockPin& getOutputPin(unsigned cntr);

	void serialize(Archive auto& ar, unsigned version);

private:
	void readBackHelper(byte value, unsigned cntr, EmuTime::param time);

private:
	std::array<Counter, 3> counter;
};

} // namespace openmsx

#endif
