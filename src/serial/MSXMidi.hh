// $Id$

#ifndef __MSXMIDI_HH__
#define __MSXMIDI_HH__

#include "MSXIODevice.hh"
#include "I8254.hh"
#include "ClockPin.hh"
#include "IRQHelper.hh"


class MSXMidi : public MSXIODevice
{
	public:
		MSXMidi(Device *config, const EmuTime &time);
		virtual ~MSXMidi();

		virtual void powerOff(const EmuTime &time);
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		void timerIRQ(bool status);

		class Counter0 : public ClockPinListener {
		public:
			virtual void signal(ClockPin& pin,
					    const EmuTime& time);
			virtual void signalPosEdge(ClockPin& pin,
						   const EmuTime& time);
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
		
		I8254 i8254;
		bool timerIRQlatch;
		bool timerIRQmasked;
		IRQHelper midiIRQ;
};

#endif
