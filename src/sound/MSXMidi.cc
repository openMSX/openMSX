// $Id$

#include "MSXMidi.hh"


MSXMidi::MSXMidi(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
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
}

byte MSXMidi::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port & 0x07) {
		case 0: // UART data register
			result = 0xFF;
			break;
		case 1: // UART status register
			result = 0x07;
			break;
		case 2: // timer interrupt flag off
		case 3: 
			result = 0xFF;
			break;
		case 4: // counter 0 data port
			result = 0xFF;
			break;
		case 5: // counter 1 data port
			result = 0xFF;
			break;
		case 6: // counter 2 data port
			result = 0xFF;
			break;
		case 7: // timer command register
			result = 0xFF;
			break;
		default:
			assert(false);
			result = 0xFF;	// avoid warning
	}
	// PRT_DEBUG("MSX-Midi read " << (int)port << " " << (int)result);
	return result;
}

void MSXMidi::writeIO(byte port, byte value, const EmuTime &time)
{
	// PRT_DEBUG("MSX-Midi write " << (int)port << " " << (int)value);
	switch (port & 0x07) {
		case 0: // UART data register
			break;
		case 1: // UART command register
			break;
		case 2: // timer interrupt flag off
		case 3: 
			break;
		case 4: // counter 0 data port
			break;
		case 5: // counter 1 data port
			break;
		case 6: // counter 2 data port
			break;
		case 7: // timer command register
			break;
	}
}
