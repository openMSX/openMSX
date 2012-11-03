// $Id$

#include "MSXMidi.hh"
#include "MidiInDevice.hh"
#include "I8254.hh"
#include "I8251.hh"
#include "MidiOutConnector.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <cassert>

namespace openmsx {

class MSXMidiCounter0 : public ClockPinListener
{
public:
	explicit MSXMidiCounter0(MSXMidi& midi);
	virtual ~MSXMidiCounter0();
	virtual void signal(ClockPin& pin, EmuTime::param time);
	virtual void signalPosEdge(ClockPin& pin, EmuTime::param time);
private:
	MSXMidi& midi;
};

class MSXMidiCounter2 : public ClockPinListener
{
public:
	explicit MSXMidiCounter2(MSXMidi& midi);
	virtual ~MSXMidiCounter2();
	virtual void signal(ClockPin& pin, EmuTime::param time);
	virtual void signalPosEdge(ClockPin& pin, EmuTime::param time);
private:
	MSXMidi& midi;
};

class MSXMidiI8251Interf : public I8251Interface
{
public:
	explicit MSXMidiI8251Interf(MSXMidi& midi);
	virtual ~MSXMidiI8251Interf();
	virtual void setRxRDY(bool status, EmuTime::param time);
	virtual void setDTR(bool status, EmuTime::param time);
	virtual void setRTS(bool status, EmuTime::param time);
	virtual bool getDSR(EmuTime::param time);
	virtual bool getCTS(EmuTime::param time);
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, EmuTime::param time);
	virtual void signal(EmuTime::param time);
private:
	MSXMidi& midi;
};


MSXMidi::MSXMidi(const DeviceConfig& config)
	: MSXDevice(config)
	, MidiInConnector(MSXDevice::getPluggingController(), "msx-midi-in")
	, cntr0(new MSXMidiCounter0(*this))
	, cntr2(new MSXMidiCounter2(*this))
	, interf(new MSXMidiI8251Interf(*this))
	, timerIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQtimer")
	, rxrdyIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQrxrdy")
	, timerIRQlatch(false), timerIRQenabled(false)
	, rxrdyIRQlatch(false), rxrdyIRQenabled(false)
	, outConnector(new MidiOutConnector(MSXDevice::getPluggingController(),
	                                    "msx-midi-out"))
	, i8251(new I8251(getScheduler(), *interf, getCurrentTime()))
	, i8254(new I8254(getScheduler(),
	                  cntr0.get(), nullptr, cntr2.get(), getCurrentTime()))
{
	EmuDuration total(1.0 / 4e6); // 4MHz
	EmuDuration hi   (1.0 / 8e6); // 8MHz half clock period
	EmuTime::param time = getCurrentTime();
	i8254->getClockPin(0).setPeriodicState(total, hi, time);
	i8254->getClockPin(1).setState(false, time);
	i8254->getClockPin(2).setPeriodicState(total, hi, time);
	i8254->getOutputPin(2).generateEdgeSignals(true, time);
	reset(time);
}

MSXMidi::~MSXMidi()
{
}

void MSXMidi::reset(EmuTime::param /*time*/)
{
	timerIRQlatch = false;
	timerIRQenabled = false;
	timerIRQ.reset();
	rxrdyIRQlatch = false;
	rxrdyIRQenabled = false;
	rxrdyIRQ.reset();
}

byte MSXMidi::readIO(word port, EmuTime::param time)
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			result = i8251->readIO(port, time);
			break;
		case 2: // timer interrupt flag off
		case 3: // no function
			result = 0xFF;
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			result = i8254->readIO(port - 4, time);
			break;
		default:
			UNREACHABLE; return 0;
	}
	return result;
}

byte MSXMidi::peekIO(word port, EmuTime::param time) const
{
	byte result;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			result = i8251->peekIO(port, time);
			break;
		case 2: // timer interrupt flag off
		case 3: // no function
			result = 0xFF;
			break;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			result = i8254->peekIO(port - 4, time);
			break;
		default:
			UNREACHABLE; return 0;
	}
	return result;
}

void MSXMidi::writeIO(word port, byte value, EmuTime::param time)
{
	port &= 0x07;
	switch (port & 0x07) {
		case 0: // UART data register
		case 1: // UART command register
			i8251->writeIO(port, value, time);
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
			i8254->writeIO(port - 4, value, time);
			break;
	}
}

void MSXMidi::setTimerIRQ(bool status, EmuTime::param time)
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

void MSXMidi::enableTimerIRQ(bool enabled, EmuTime::param time)
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

void MSXMidi::updateEdgeEvents(EmuTime::param time)
{
	bool wantEdges = timerIRQenabled && !timerIRQlatch;
	i8254->getOutputPin(2).generateEdgeSignals(wantEdges, time);
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

MSXMidiI8251Interf::MSXMidiI8251Interf(MSXMidi& midi_)
	: midi(midi_)
{
}

MSXMidiI8251Interf::~MSXMidiI8251Interf()
{
}

void MSXMidiI8251Interf::setRxRDY(bool status, EmuTime::param /*time*/)
{
	midi.setRxRDYIRQ(status);
}

void MSXMidiI8251Interf::setDTR(bool status, EmuTime::param time)
{
	midi.enableTimerIRQ(status, time);
}

void MSXMidiI8251Interf::setRTS(bool status, EmuTime::param /*time*/)
{
	midi.enableRxRDYIRQ(status);
}

bool MSXMidiI8251Interf::getDSR(EmuTime::param /*time*/)
{
	return midi.timerIRQ.getState();
}

bool MSXMidiI8251Interf::getCTS(EmuTime::param /*time*/)
{
	return true;
}

void MSXMidiI8251Interf::setDataBits(DataBits bits)
{
	midi.outConnector->setDataBits(bits);
}

void MSXMidiI8251Interf::setStopBits(StopBits bits)
{
	midi.outConnector->setStopBits(bits);
}

void MSXMidiI8251Interf::setParityBit(bool enable, ParityBit parity)
{
	midi.outConnector->setParityBit(enable, parity);
}

void MSXMidiI8251Interf::recvByte(byte value, EmuTime::param time)
{
	midi.outConnector->recvByte(value, time);
}

void MSXMidiI8251Interf::signal(EmuTime::param time)
{
	midi.getPluggedMidiInDev().signal(time);
}


// Counter 0 output

MSXMidiCounter0::MSXMidiCounter0(MSXMidi& midi_)
	: midi(midi_)
{
}

MSXMidiCounter0::~MSXMidiCounter0()
{
}

void MSXMidiCounter0::signal(ClockPin& pin, EmuTime::param time)
{
	ClockPin& clk = midi.i8251->getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXMidiCounter0::signalPosEdge(ClockPin& /*pin*/, EmuTime::param /*time*/)
{
	UNREACHABLE;
}


// Counter 2 output

MSXMidiCounter2::MSXMidiCounter2(MSXMidi& midi_)
	: midi(midi_)
{
}

MSXMidiCounter2::~MSXMidiCounter2()
{
}

void MSXMidiCounter2::signal(ClockPin& pin, EmuTime::param time)
{
	ClockPin& clk = midi.i8254->getClockPin(1);
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXMidiCounter2::signalPosEdge(ClockPin& /*pin*/, EmuTime::param time)
{
	midi.setTimerIRQ(true, time);
}


// MidiInConnector

bool MSXMidi::ready()
{
	return i8251->isRecvReady();
}

bool MSXMidi::acceptsData()
{
	return i8251->isRecvEnabled();
}

void MSXMidi::setDataBits(DataBits bits)
{
	i8251->setDataBits(bits);
}

void MSXMidi::setStopBits(StopBits bits)
{
	i8251->setStopBits(bits);
}

void MSXMidi::setParityBit(bool enable, ParityBit parity)
{
	i8251->setParityBit(enable, parity);
}

void MSXMidi::recvByte(byte value, EmuTime::param time)
{
	i8251->recvByte(value, time);
}


template<typename Archive>
void MSXMidi::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	ar.template serializeBase<MidiInConnector>(*this);
	ar.serialize("outConnector", *outConnector);

	ar.serialize("timerIRQ", timerIRQ);
	ar.serialize("rxrdyIRQ", rxrdyIRQ);
	ar.serialize("timerIRQlatch", timerIRQlatch);
	ar.serialize("timerIRQenabled", timerIRQenabled);
	ar.serialize("rxrdyIRQlatch", rxrdyIRQlatch);
	ar.serialize("rxrdyIRQenabled", rxrdyIRQenabled);
	ar.serialize("I8251", *i8251);
	ar.serialize("I8254", *i8254);

	// don't serialize:  cntr0, cntr2, interf
}
INSTANTIATE_SERIALIZE_METHODS(MSXMidi);
REGISTER_MSXDEVICE(MSXMidi, "MSX-Midi");

} // namespace openmsx

