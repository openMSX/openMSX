// $Id$

#include "MSXMidi.hh"
#include <cassert>


MSXMidi::MSXMidi(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  cntr0(*this), cntr2(*this),
	  i8251(this, time), i8254(&cntr0, NULL, &cntr2, time)
{
	EmuDuration total(1.0 / 4e6); // 4MHz
	EmuDuration hi   (1.0 / 8e6); // 8MHz half clock period
	i8254.getClockPin(0).setPeriodicState(total, hi, time);
	i8254.getClockPin(1).setState(false, time);
	i8254.getClockPin(2).setPeriodicState(total, hi, time);
	i8254.getOutputPin(2).generateEdgeSignals(true, time);
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
	timerIRQenabled = false;
	rxrdyIRQlatch = false;
	rxrdyIRQenabled = false;
}

byte MSXMidi::readIO(byte port, const EmuTime &time)
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			result = i8251.readIO(port, time);
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
	//PRT_DEBUG("MSX-Midi read " << (int)port << " " << (int)result);
	return result;
}

void MSXMidi::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG("MSX-Midi write " << (int)port << " " << (int)value);
	port &= 0x07;
	switch (port & 0x07) {
		case 0: // UART data register
		case 1: // UART command register
			i8251.writeIO(port, value, time);
			break;
		case 2: // timer interrupt flag off
			setTimerIRQ(false);
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

void MSXMidi::setTimerIRQ(bool status)
{
	if (timerIRQlatch != status) {
		timerIRQlatch = status;
		if (timerIRQenabled) {
			if (timerIRQlatch) {
				timerIRQ.set();
			} else {
				timerIRQ.reset();
			}
		}
	}
}

void MSXMidi::enableTimerIRQ(bool enabled)
{
	// TODO optimize generateEdge
	if (timerIRQenabled != enabled) {
		timerIRQenabled = enabled;
		if (timerIRQlatch) {
			if (timerIRQenabled) {
				timerIRQ.set();
			} else {
				timerIRQ.reset();
			}
		}
	}
}

void MSXMidi::setRxRDYIRQ(bool status)
{
	if (rxrdyIRQlatch != status) {
		rxrdyIRQlatch = status;
		if (rxrdyIRQenabled) {
			if (rxrdyIRQlatch) {
				rxrdyIRQ.set();
			} else {
				rxrdyIRQ.reset();
			}
		}
	}
}

void MSXMidi::enableRxRDYIRQ(bool enabled)
{
	if (rxrdyIRQenabled != enabled) {
		rxrdyIRQenabled = enabled;
		if (!rxrdyIRQenabled && rxrdyIRQlatch) {
			rxrdyIRQ.reset();
		}
	}
}


// I8251Interface

void MSXMidi::setRxRDY(bool status, const EmuTime& time)
{
	setRxRDYIRQ(status);
}

void MSXMidi::setDTR(bool status, const EmuTime& time)
{
	enableTimerIRQ(status);
}

void MSXMidi::setRTS(bool status, const EmuTime& time)
{
	enableRxRDYIRQ(status);
}

byte MSXMidi::getDSR(const EmuTime& time)
{
	return timerIRQ.getState();
}


// Counter 0 output

MSXMidi::Counter0::Counter0(MSXMidi& midi_)
	: midi(midi_)
{
}

MSXMidi::Counter0::~Counter0()
{
}

void MSXMidi::Counter0::signal(ClockPin& pin, const EmuTime& time)
{
	ClockPin& clk = midi.i8251.getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
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
	ClockPin& clk = midi.i8254.getClockPin(1);
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXMidi::Counter2::signalPosEdge(ClockPin& pin, const EmuTime& time)
{
	midi.setTimerIRQ(true);
}
