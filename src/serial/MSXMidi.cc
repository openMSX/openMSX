// $Id$

#include "MSXMidi.hh"
#include <cassert>


MSXMidi::MSXMidi(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  cntr2(*this), i8254(&cntr0, NULL, &cntr2, time)
{
	i8254.getClockPin(2).generateEdgeSignals(true, time);
	reset(time);
}

MSXMidi::~MSXMidi()
{
}

void MSXMidi::powerOff(const EmuTime &time)
{
}

void MSXMidi::reset(const EmuTime &time)
{
	timerIRQlatch = false;
	timerIRQmasked = false;
}

byte MSXMidi::readIO(byte port, const EmuTime &time)
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
			result = 0xFF;
			break;
		case 1: // UART status register
			result = 0x07;
			break;
		case 2: // timer interrupt flag off
		case 3: // no function
			result = 0xFF;
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			result = i8254.readIO(port - 4, time);
			break;
		default:
			assert(false);
			result = 0xFF;	// avoid warning
	}
	PRT_DEBUG("MSX-Midi read " << (int)port << " " << (int)result);
	return result;
}

void MSXMidi::writeIO(byte port, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSX-Midi write " << (int)port << " " << (int)value);
	port &= 0x07;
	switch (port & 0x07) {
		case 0: // UART data register
			break;
		case 1: // UART command register
			break;
		case 2: // timer interrupt flag off
			timerIRQ(false);
			break;
		case 3: // no function
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			i8254.writeIO(port - 4, value, time);
			break;
	}
}

void MSXMidi::timerIRQ(bool status)
{
	if (timerIRQlatch != status) {
		timerIRQlatch = status;
		if (!timerIRQmasked) {
			if (status) {
				midiIRQ.set();
				// set DSR
			} else {
				midiIRQ.reset();
				// reset DSR
			}
		}
	}
}


// Counter 0 output

void MSXMidi::Counter0::signal(ClockPin& pin, const EmuTime& time)
{
	if (pin.isPeriodic()) {
		// TODO set baud rate
	} else {
		// TODO
	}
}

void MSXMidi::Counter0::signalPosEdge(ClockPin& pin, const EmuTime& time)
{
	assert(false);
}


// Counter 2 output

MSXMidi::Counter2::Counter2(MSXMidi& midi_)
	: midi(midi_)
{
}

MSXMidi::Counter2::~Counter2()
{
}

void MSXMidi::Counter2::signal(ClockPin& pin, const EmuTime& time)
{
	// nothing
}

void MSXMidi::Counter2::signalPosEdge(ClockPin& pin, const EmuTime& time)
{
	midi.timerIRQ(true);
}
