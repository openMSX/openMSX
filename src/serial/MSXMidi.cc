#include "MSXMidi.hh"
#include "MidiInDevice.hh"
#include "I8254.hh"
#include "I8251.hh"
#include "MidiOutConnector.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <cassert>

namespace openmsx {

// Documented in MSX-Datapack Vol. 3, section 4 (MSX-MIDI), from page 634
static const byte LIMITED_RANGE_VALUE = 0x01; // b0 = "E8" => determines port range
static const byte DISABLED_VALUE      = 0x80; // b7 = EN

class MSXMidiCounter0 final : public ClockPinListener
{
public:
	explicit MSXMidiCounter0(MSXMidi& midi);
	~MSXMidiCounter0();
	void signal(ClockPin& pin, EmuTime::param time) override;
	void signalPosEdge(ClockPin& pin, EmuTime::param time) override;
private:
	MSXMidi& midi;
};

class MSXMidiCounter2 final : public ClockPinListener
{
public:
	explicit MSXMidiCounter2(MSXMidi& midi);
	~MSXMidiCounter2();
	void signal(ClockPin& pin, EmuTime::param time) override;
	void signalPosEdge(ClockPin& pin, EmuTime::param time) override;
private:
	MSXMidi& midi;
};

class MSXMidiI8251Interf final : public I8251Interface
{
public:
	explicit MSXMidiI8251Interf(MSXMidi& midi);
	~MSXMidiI8251Interf();
	void setRxRDY(bool status, EmuTime::param time) override;
	void setDTR(bool status, EmuTime::param time) override;
	void setRTS(bool status, EmuTime::param time) override;
	bool getDSR(EmuTime::param time) override;
	bool getCTS(EmuTime::param time) override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
	void recvByte(byte value, EmuTime::param time) override;
	void signal(EmuTime::param time) override;
private:
	MSXMidi& midi;
};


MSXMidi::MSXMidi(const DeviceConfig& config)
	: MSXDevice(config)
	, MidiInConnector(MSXDevice::getPluggingController(), "msx-midi-in")
	, cntr0(make_unique<MSXMidiCounter0>(*this))
	, cntr2(make_unique<MSXMidiCounter2>(*this))
	, interf(make_unique<MSXMidiI8251Interf>(*this))
	, timerIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQtimer")
	, rxrdyIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQrxrdy")
	, timerIRQlatch(false), timerIRQenabled(false)
	, rxrdyIRQlatch(false), rxrdyIRQenabled(false)
	, isExternalMSXMIDI(config.findChild("external") != nullptr)
	, isEnabled(!isExternalMSXMIDI)
	, isLimitedTo8251(true)
	, outConnector(make_unique<MidiOutConnector>(
		MSXDevice::getPluggingController(), "msx-midi-out"))
	, i8251(make_unique<I8251>(getScheduler(), *interf, getCurrentTime()))
	, i8254(make_unique<I8254>(
		getScheduler(), cntr0.get(), nullptr, cntr2.get(),
		getCurrentTime()))
{
	EmuDuration total(1.0 / 4e6); // 4MHz
	EmuDuration hi   (1.0 / 8e6); // 8MHz half clock period
	EmuTime::param time = getCurrentTime();
	i8254->getClockPin(0).setPeriodicState(total, hi, time);
	i8254->getClockPin(1).setState(false, time);
	i8254->getClockPin(2).setPeriodicState(total, hi, time);
	i8254->getOutputPin(2).generateEdgeSignals(true, time);
	reset(time);

	if (isExternalMSXMIDI) {
		// Ports are dynamically registered.
		if (config.findChild("io")) {
			throw MSXException(
				"Bad MSX-MIDI configuration, when using "
				"'external', you cannot specify I/O ports!");
		}
		// Out-port 0xE2 is always enabled.
		getCPUInterface().register_IO_Out(0xE2, this);
		// Not enabled, so no other ports need to be registered yet.
		assert(!isEnabled);
	} else {
		// Ports are statically registered (via the config file).
	}
}

MSXMidi::~MSXMidi()
{
	if (isExternalMSXMIDI) {
		registerIOports(DISABLED_VALUE | LIMITED_RANGE_VALUE); // disabled, unregisters ports
		getCPUInterface().unregister_IO_Out(0xE2, this);
	}
}

void MSXMidi::reset(EmuTime::param /*time*/)
{
	timerIRQlatch = false;
	timerIRQenabled = false;
	timerIRQ.reset();
	rxrdyIRQlatch = false;
	rxrdyIRQenabled = false;
	rxrdyIRQ.reset();

	if (isExternalMSXMIDI) {
		registerIOports(DISABLED_VALUE | LIMITED_RANGE_VALUE); // also resets state
	}
}

byte MSXMidi::readIO(word port, EmuTime::param time)
{
	// If not enabled then no ports should have been registered.
	assert(isEnabled);

	// This handles both ports in range 0xE0-0xE1 and 0xE8-0xEF
	// Depending on the value written to 0xE2 they are active or not
	switch (port & 7) {
		case 0: // UART data register
		case 1: // UART status register
			return i8251->readIO(port & 1, time);
		case 2: // timer interrupt flag off
		case 3: // no function
			return 0xFF;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			return i8254->readIO(port & 3, time);
		default:
			UNREACHABLE; return 0;
	}
}

byte MSXMidi::peekIO(word port, EmuTime::param time) const
{
	// If not enabled then no ports should have been registered.
	assert(isEnabled);

	// This handles both ports in range 0xE0-0xE1 and 0xE8-0xEF
	// Depending on the value written to 0xE2 they are active or not
	switch (port & 7) {
		case 0: // UART data register
		case 1: // UART status register
			return i8251->peekIO(port & 1, time);
		case 2: // timer interrupt flag off
		case 3: // no function
			return 0xFF;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			return i8254->peekIO(port & 3, time);
		default:
			UNREACHABLE; return 0;
	}
}

void MSXMidi::writeIO(word port, byte value, EmuTime::param time)
{
	if (isExternalMSXMIDI && ((port & 0xFF) == 0xE2)) {
		// control register
		registerIOports(value);
		return;
	}
	assert(isEnabled);

	// This handles both ports in range 0xE0-0xE1 and 0xE8-0xEF
	switch (port & 7) {
		case 0: // UART data register
		case 1: // UART command register
			i8251->writeIO(port & 1, value, time);
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
			i8254->writeIO(port & 3, value, time);
			break;
	}
}

void MSXMidi::registerIOports(byte value)
{
	assert(isExternalMSXMIDI);
	bool newIsEnabled = (value & DISABLED_VALUE) == 0;
	bool newIsLimited = (value & LIMITED_RANGE_VALUE) != 0;

	if (newIsEnabled != isEnabled) {
		// Enable/disabled status changes, possibly limited status
		// changes as well but that doesn't matter, we anyway need
		// to (un)register the full port range.
		if (newIsEnabled) {
			// disabled -> enabled
			if (newIsLimited) {
				registerRange(0xE0, 2);
			} else {
				registerRange(0xE8, 8);
			}
		} else {
			// enabled -> disabled
			if (isLimitedTo8251) { // note: old isLimited status
				unregisterRange(0xE0, 2);
			} else {
				unregisterRange(0xE8, 8);
			}
		}

	} else if (isEnabled && (newIsLimited != isLimitedTo8251)) {
		// Remains enabled, and only 'isLimited' status changes.
		// Need to switch between the low/high range.
		if (newIsLimited) {
			// Switch high->low range.
			unregisterRange(0xE8, 8);
			registerRange  (0xE0, 2);
		} else {
			// Switch low->high range.
			unregisterRange(0xE0, 2);
			registerRange  (0xE8, 8);
		}
	}

	isEnabled       = newIsEnabled;
	isLimitedTo8251 = newIsLimited;
}

void MSXMidi::registerRange(byte port, unsigned num)
{
	for (unsigned i = 0; i < num; ++i) {
		getCPUInterface().register_IO_In (port + i, this);
		getCPUInterface().register_IO_Out(port + i, this);
	}
}
void MSXMidi::unregisterRange(byte port, unsigned num)
{
	for (unsigned i = 0; i < num; ++i) {
		getCPUInterface().unregister_IO_In (port + i, this);
		getCPUInterface().unregister_IO_Out(port + i, this);
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
void MSXMidi::serialize(Archive& ar, unsigned version)
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
	if (ar.versionAtLeast(version, 2)) {
		bool newIsEnabled = isEnabled; // copy for saver
		bool newIsLimitedTo8251 = isLimitedTo8251; // copy for saver
		ar.serialize("isEnabled", newIsEnabled);
		ar.serialize("isLimitedTo8251", newIsLimitedTo8251);
		if (ar.isLoader() && isExternalMSXMIDI) {
			registerIOports((newIsEnabled ? 0x00 : DISABLED_VALUE) | (newIsLimitedTo8251 ? LIMITED_RANGE_VALUE : 0x00));
		}
	}

	// don't serialize:  cntr0, cntr2, interf
}
INSTANTIATE_SERIALIZE_METHODS(MSXMidi);
REGISTER_MSXDEVICE(MSXMidi, "MSX-Midi");

} // namespace openmsx
