// $Id$

#ifndef __MSXMIDI_HH__
#define __MSXMIDI_HH__

#include "MSXIODevice.hh"
#include "I8251.hh"
#include "I8254.hh"
#include "ClockPin.hh"
#include "IRQHelper.hh"


class MSXMidi : public MSXIODevice, private I8251Interface
{
	public:
		MSXMidi(Device *config, const EmuTime &time);
		virtual ~MSXMidi();

		virtual void powerOff(const EmuTime &time);
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		void setTimerIRQ(bool status);
		void enableTimerIRQ(bool enabled);
		void setRxRDYIRQ(bool status);
		void enableRxRDYIRQ(bool enabled);
		
		// I8251Interface
		virtual void setRxRDY(bool status, const EmuTime& time);
		virtual void setDTR(bool status, const EmuTime& time);
		virtual void setRTS(bool status, const EmuTime& time);
		virtual byte getDSR(const EmuTime& time);

		class Counter0 : public ClockPinListener {
		public:
			Counter0(MSXMidi& midi);
			virtual ~Counter0();
			virtual void signal(ClockPin& pin,
					    const EmuTime& time);
			virtual void signalPosEdge(ClockPin& pin,
						   const EmuTime& time);
		private:
			MSXMidi& midi;
		} cntr0;

		class Counter2 : public ClockPinListener {
		public:
			Counter2(MSXMidi& midi);
			virtual ~Counter2();
			virtual void signal(ClockPin& pin,
					    const EmuTime& time);
			virtual void signalPosEdge(ClockPin& pin,
						   const EmuTime& time);
		private:
			MSXMidi& midi;
		} cntr2;
		
		I8251 i8251;
		I8254 i8254;
		
		bool timerIRQlatch;
		bool timerIRQenabled;
		IRQHelper timerIRQ;
		bool rxrdyIRQlatch;
		bool rxrdyIRQenabled;
		IRQHelper rxrdyIRQ;
};

#endif
