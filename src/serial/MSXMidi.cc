// $Id$

#include <cassert>
#include "MSXMidi.hh"
#include "MidiInDevice.hh"

namespace openmsx {

MSXMidi::MSXMidi(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
	, MidiInConnector("msx-midi-in")
	, timerIRQlatch(false), timerIRQenabled(false)
	, rxrdyIRQlatch(false), rxrdyIRQenabled(false)
	, cntr0(*this), cntr2(*this), i8254(&cntr0, NULL, &cntr2, time)
	, interf(*this), i8251(&interf, time)
	, outConnector("msx-midi-out")
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

void MSXMidi::reset(const EmuTime& /*time*/)
{
	timerIRQlatch = false;
	timerIRQenabled = false;
	timerIRQ.reset();
	rxrdyIRQlatch = false;
	rxrdyIRQenabled = false;
	rxrdyIRQ.reset();
}

byte MSXMidi::readIO(byte port, const EmuTime& time)
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

byte MSXMidi::peekIO(byte port, const EmuTime& time) const
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			result = i8251.peekIO(port, time);
			break;
		case 2: // timer interrupt flag off
		case 3: // no function
			result = 0xFF;
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			result = i8254.peekIO(port - 4, time);
			break;
		default:
			assert(false);
			result = 0xFF;	// avoid warning
	}
	return result;
}

void MSXMidi::writeIO(byte port, byte value, const EmuTime& time)
{
	//PRT_DEBUG("MSX-Midi write " << (int)port << " " << (int)value);
	port &= 0x07;
	switch (port & 0x07) {
		case 0: // UART data register
		case 1: // UART command register
			i8251.writeIO(port, value, time);
			break;
		case 2: // timer interrupt flag off
			setTimerIRQ(false, time);
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

void MSXMidi::setTimerIRQ(bool status, const EmuTime& time)
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
		updateEdgeEvents(time);
	}
}

void MSXMidi::enableTimerIRQ(bool enabled, const EmuTime& time)
{
	if (timerIRQenabled != enabled) {
		timerIRQenabled = enabled;
		if (timerIRQlatch) {
			if (timerIRQenabled) {
				timerIRQ.set();
			} else {
				timerIRQ.reset();
			}
		}
		updateEdgeEvents(time);
	}
}

void MSXMidi::updateEdgeEvents(const EmuTime& time)
{
	bool wantEdges = timerIRQenabled && !timerIRQlatch;
	i8254.getOutputPin(2).generateEdgeSignals(wantEdges, time);
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


// I8251Interface  (pass calls from I8251 to outConnector)

MSXMidi::I8251Interf::I8251Interf(MSXMidi& midi_)
	: midi(midi_)
{
}

MSXMidi::I8251Interf::~I8251Interf()
{
}

void MSXMidi::I8251Interf::setRxRDY(bool status, const EmuTime& /*time*/)
{
	midi.setRxRDYIRQ(status);
}

void MSXMidi::I8251Interf::setDTR(bool status, const EmuTime& time)
{
	midi.enableTimerIRQ(status, time);
}

void MSXMidi::I8251Interf::setRTS(bool status, const EmuTime& /*time*/)
{
	midi.enableRxRDYIRQ(status);
}

bool MSXMidi::I8251Interf::getDSR(const EmuTime& /*time*/)
{
	return midi.timerIRQ.getState();
}

bool MSXMidi::I8251Interf::getCTS(const EmuTime& /*time*/)
{
	return true;
}

void MSXMidi::I8251Interf::setDataBits(DataBits bits)
{
	midi.outConnector.setDataBits(bits);
}

void MSXMidi::I8251Interf::setStopBits(StopBits bits)
{
	midi.outConnector.setStopBits(bits);
}

void MSXMidi::I8251Interf::setParityBit(bool enable, ParityBit parity)
{
	midi.outConnector.setParityBit(enable, parity);
}

void MSXMidi::I8251Interf::recvByte(byte value, const EmuTime& time)
{
	midi.outConnector.recvByte(value, time);
}

void MSXMidi::I8251Interf::signal(const EmuTime& time)
{
	midi.getPlugged().signal(time);
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

void MSXMidi::Counter0::signalPosEdge(ClockPin& /*pin*/,
                                      const EmuTime& /*time*/)
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

void MSXMidi::Counter2::signalPosEdge(ClockPin& /*pin*/, const EmuTime& time)
{
	midi.setTimerIRQ(true, time);
}


// MidiInConnector

bool MSXMidi::ready()
{
	return i8251.isRecvReady();
}

bool MSXMidi::acceptsData()
{
	return i8251.isRecvEnabled();
}

void MSXMidi::setDataBits(DataBits bits)
{
	i8251.setDataBits(bits);
}

void MSXMidi::setStopBits(StopBits bits)
{
	i8251.setStopBits(bits);
}

void MSXMidi::setParityBit(bool enable, ParityBit parity)
{
	i8251.setParityBit(enable, parity);
}

void MSXMidi::recvByte(byte value, const EmuTime& time)
{
	i8251.recvByte(value, time);
}

} // namespace openmsx

