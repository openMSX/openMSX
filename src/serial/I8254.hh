// $Id$

// This class implements the Intel 8254 chip (and 8253)
//
// * Only the 8254 is emulated, no surrounding hardware. 
//   Use the class I8254Interface to do that.

#ifndef __I8254_HH__
#define __I8254_HH__

#include "openmsx.hh"
#include "EmuTime.hh"
#include "ClockPin.hh"

namespace openmsx {

class I8254
{
	class Counter {
	public:
		Counter(ClockPinListener* listener, const EmuTime& time);
		void reset(const EmuTime& time);
		byte readIO(const EmuTime& time);
		byte peekIO(const EmuTime& time) const;
		void writeIO(byte value, const EmuTime& time);
		void setGateStatus(bool status, const EmuTime& time);
		void writeControlWord(byte value, const EmuTime& time);
		void latchStatus(const EmuTime& time);
		void latchCounter(const EmuTime& time);
		
	private:
		enum ByteOrder {LOW, HIGH};
		static const byte WRT_FRMT = 0x30;
		static const byte WF_LATCH = 0x00;
		static const byte WF_LOW   = 0x10;
		static const byte WF_HIGH  = 0x20;
		static const byte WF_BOTH  = 0x30;
		static const byte CNTR_MODE = 0x0E;
		static const byte CNTR_M0   = 0x00;
		static const byte CNTR_M1   = 0x02;
		static const byte CNTR_M2   = 0x04;
		static const byte CNTR_M3   = 0x06;
		static const byte CNTR_M4   = 0x08;
		static const byte CNTR_M5   = 0x0A;
		static const byte CNTR_M2_  = 0x0C;
		static const byte CNTR_M3_  = 0x0E;

		void writeLoad(word value, const EmuTime& time);
		void advance(const EmuTime& time);

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

public:
	I8254(ClockPinListener* output0, ClockPinListener* output1,
	      ClockPinListener* output2, const EmuTime& time); 
	~I8254(); 
	
	void reset(const EmuTime& time);
	byte readIO(byte port, const EmuTime& time);
	byte peekIO(byte port, const EmuTime& time) const;
	void writeIO(byte port, byte value, const EmuTime& time);
	
	void setGate(byte counter, bool status, const EmuTime& time);
	ClockPin& getClockPin(byte cntr);
	ClockPin& getOutputPin(byte cntr);

private:
	static const byte READ_BACK = 0xC0;
	static const byte RB_CNTR0  = 0x02;
	static const byte RB_CNTR1  = 0x04;
	static const byte RB_CNTR2  = 0x08;
	static const byte RB_STATUS = 0x10;
	static const byte RB_COUNT  = 0x20;
	
	void readBackHelper(byte value, byte cntr, const EmuTime& time);
	Counter& getCounter(byte cntr);

	//Counter counter[3];
	Counter counter0;
	Counter counter1;
	Counter counter2;
};

} // namespace openmsx
#endif
