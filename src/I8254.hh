// $Id: 

// This class implements the Intel 8254 chip (and 8253)
//
// * Only the 8254 is emulated, no surrounding hardware. 
//   Use the class I8254Interface to do that.
// * For techncal details on 8254 see
//    http://w3.qahwah.net/joost/openMSX/8254.pdf

#ifndef __I8254_HH__
#define __I8254_HH__

#include "openmsx.hh"
#include "EmuTime.hh"


class I8254Interface
{
	public:
		virtual void output0(bool status) {}
		virtual void output1(bool status) {}
		virtual void output2(bool status) {}
};

class I8254
{
	class Counter {
		public:
			Counter(int freq, I8254 *parent, int id);
			void reset();
			byte readIO(const EmuTime &time);
			void writeIO(byte value, const EmuTime &time);
			void setGateStatus(bool status, const EmuTime &time);
			bool getOutput(const EmuTime &time);
			void writeControlWord(byte value, const EmuTime &time);
			void latchStatus(const EmuTime &time);
			void latchCounter(const EmuTime &time);
			void advance(const EmuTime &time);
		private:
			enum ByteOrder {LOW, HIGH};
			static const byte WRT_FRMT = 0x30;
			static const byte WF_LATCH = 0x00;
			static const byte WF_LOW   = 0x10;
			static const byte WF_HIGH  = 0x20;
			static const byte WF_BOTH  = 0x30;
			static const byte CNTR_MODE = 0x0e;
			static const byte CNTR_M0   = 0x00;
			static const byte CNTR_M1   = 0x02;
			static const byte CNTR_M2   = 0x04;
			static const byte CNTR_M3   = 0x06;
			static const byte CNTR_M4   = 0x08;
			static const byte CNTR_M5   = 0x0a;
			static const byte CNTR_M2_  = 0x0c;
			static const byte CNTR_M3_  = 0x0e;
	
			void writeLoad(word value);
			void changeOutput(bool newOutput);

			EmuTime currentTime;
			I8254 *parent;
			int id;
			int counter;
			word latchedCounter, counterLoad;
			byte control, latchedControl;
			bool ltchCtrl, ltchCntr;
			ByteOrder readOrder, writeOrder;
			byte writeLatch;
			bool output, gate;
			bool active, triggered, counting;
	};
	
	public:
		static const int NR_COUNTERS = 3;
	
		I8254(int freq[NR_COUNTERS], I8254Interface *interf); 
		~I8254(); 
		
		void reset();

		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
		void setGateStatus(byte counter, bool status, const EmuTime &time);
		bool getOutput(byte counter, const EmuTime &time);

		void outputCallback(int id, bool status);

	private:
		static const byte READ_BACK = 0xc0;
		static const byte RB_CNTR0  = 0x02;
		static const byte RB_CNTR1  = 0x04;
		static const byte RB_CNTR2  = 0x08;
		static const byte RB_STATUS = 0x10;
		static const byte RB_COUNT  = 0x20;
		
		void readBackHelper(byte value, byte cntr, const EmuTime &time);
	
		I8254Interface *interface;
		Counter* counter[NR_COUNTERS];
};
#endif
