// This class implements the Intel 8254 chip (and 8253)
//
// * Only the 8254 is emulated, no surrounding hardware.
//   Use the class I8254Interface to do that.

#ifndef I8254_HH
#define I8254_HH

#include "ClockPin.hh"
#include "EmuTime.hh"
#include <array>
#include <cstdint>

namespace openmsx {

class Scheduler;
class ClockPinListener;

class Counter {
public:
	Counter(Scheduler& scheduler, ClockPinListener* listener,
		EmuTime::param time);
	void reset(EmuTime::param time);
	[[nodiscard]] uint8_t readIO(EmuTime::param time);
	[[nodiscard]] uint8_t peekIO(EmuTime::param time) const;
	void writeIO(uint8_t value, EmuTime::param time);
	void setGateStatus(bool status, EmuTime::param time);
	void writeControlWord(uint8_t value, EmuTime::param time);
	void latchStatus(EmuTime::param time);
	void latchCounter(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

//private:
	enum ByteOrder {LOW, HIGH};

private:
	static constexpr uint8_t WRT_FRMT = 0x30;
	static constexpr uint8_t WF_LATCH = 0x00;
	static constexpr uint8_t WF_LOW   = 0x10;
	static constexpr uint8_t WF_HIGH  = 0x20;
	static constexpr uint8_t WF_BOTH  = 0x30;
	static constexpr uint8_t CNTR_MODE = 0x0E;
	static constexpr uint8_t CNTR_M0   = 0x00;
	static constexpr uint8_t CNTR_M1   = 0x02;
	static constexpr uint8_t CNTR_M2   = 0x04;
	static constexpr uint8_t CNTR_M3   = 0x06;
	static constexpr uint8_t CNTR_M4   = 0x08;
	static constexpr uint8_t CNTR_M5   = 0x0A;
	static constexpr uint8_t CNTR_M2_  = 0x0C;
	static constexpr uint8_t CNTR_M3_  = 0x0E;

	void writeLoad(uint16_t value, EmuTime::param time);
	void advance(EmuTime::param time);

private:
	ClockPin clock;
	ClockPin output;
	EmuTime currentTime;
	int counter = 0;
	uint16_t latchedCounter;
	uint16_t counterLoad = 0;
	uint8_t control, latchedControl;
	bool ltchCtrl, ltchCntr;
	ByteOrder readOrder, writeOrder;
	uint8_t writeLatch;
	bool gate = true;
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
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime::param time);
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime::param time) const;
	void writeIO(uint16_t port, uint8_t value, EmuTime::param time);

	void setGate(unsigned cntr, bool status, EmuTime::param time);
	[[nodiscard]] ClockPin& getClockPin(unsigned cntr);
	[[nodiscard]] ClockPin& getOutputPin(unsigned cntr);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void readBackHelper(uint8_t value, unsigned cntr, EmuTime::param time);

private:
	std::array<Counter, 3> counter;
};

} // namespace openmsx

#endif
