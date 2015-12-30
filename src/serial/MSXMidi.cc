#include "MSXMidi.hh"
#include "MidiInDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "memory.hh"
#include "outer.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <cassert>

namespace openmsx {

// Documented in MSX-Datapack Vol. 3, section 4 (MSX-MIDI), from page 634
static const byte LIMITED_RANGE_VALUE = 0x01; // b0 = "E8" => determines port range
static const byte DISABLED_VALUE      = 0x80; // b7 = EN

MSXMidi::MSXMidi(const DeviceConfig& config)
	: MSXDevice(config)
	, MidiInConnector(MSXDevice::getPluggingController(), "msx-midi-in")
	, timerIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQtimer")
	, rxrdyIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQrxrdy")
	, timerIRQlatch(false), timerIRQenabled(false)
	, rxrdyIRQlatch(false), rxrdyIRQenabled(false)
	, isExternalMSXMIDI(config.findChild("external") != nullptr)
	, isEnabled(!isExternalMSXMIDI)
	, isLimitedTo8251(true)
	, outConnector(MSXDevice::getPluggingController(), "msx-midi-out")
	, i8251(getScheduler(), interf, getCurrentTime())
	, i8254(getScheduler(), &cntr0, nullptr, &cntr2, getCurrentTime())
{
	EmuDuration total(1.0 / 4e6); // 4MHz
	EmuDuration hi   (1.0 / 8e6); // 8MHz half clock period
	EmuTime::param time = getCurrentTime();
	i8254.getClockPin(0).setPeriodicState(total, hi, time);
	i8254.getClockPin(1).setState(false, time);
	i8254.getClockPin(2).setPeriodicState(total, hi, time);
	i8254.getOutputPin(2).generateEdgeSignals(true, time);
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

void MSXMidi::reset(EmuTime::param time)
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
	i8251.reset(time);
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
			return i8251.readIO(port & 1, time);
		case 2: // timer interrupt flag off
		case 3: // no function
			return 0xFF;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			return i8254.readIO(port & 3, time);
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
			return i8251.peekIO(port & 1, time);
		case 2: // timer interrupt flag off
		case 3: // no function
			return 0xFF;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			return i8254.peekIO(port & 3, time);
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
			i8251.writeIO(port & 1, value, time);
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
			i8254.writeIO(port & 3, value, time);
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
			timerIRQ.set(timerIRQlatch);
		}
		updateEdgeEvents(time);
	}
}

void MSXMidi::enableTimerIRQ(bool enabled, EmuTime::param time)
{
	if (timerIRQenabled != enabled) {
		timerIRQenabled = enabled;
		if (timerIRQlatch) {
			timerIRQ.set(timerIRQenabled);
		}
		updateEdgeEvents(time);
	}
}

void MSXMidi::updateEdgeEvents(EmuTime::param time)
{
	bool wantEdges = timerIRQenabled && !timerIRQlatch;
	i8254.getOutputPin(2).generateEdgeSignals(wantEdges, time);
}

void MSXMidi::setRxRDYIRQ(bool status)
{
	if (rxrdyIRQlatch != status) {
		rxrdyIRQlatch = status;
		if (rxrdyIRQenabled) {
			rxrdyIRQ.set(rxrdyIRQlatch);
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

void MSXMidi::I8251Interf::setRxRDY(bool status, EmuTime::param /*time*/)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.setRxRDYIRQ(status);
}

void MSXMidi::I8251Interf::setDTR(bool status, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.enableTimerIRQ(status, time);
}

void MSXMidi::I8251Interf::setRTS(bool status, EmuTime::param /*time*/)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.enableRxRDYIRQ(status);
}

bool MSXMidi::I8251Interf::getDSR(EmuTime::param /*time*/)
{
	auto& midi = OUTER(MSXMidi, interf);
	return midi.timerIRQ.getState();
}

bool MSXMidi::I8251Interf::getCTS(EmuTime::param /*time*/)
{
	return true;
}

void MSXMidi::I8251Interf::setDataBits(DataBits bits)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.outConnector.setDataBits(bits);
}

void MSXMidi::I8251Interf::setStopBits(StopBits bits)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.outConnector.setStopBits(bits);
}

void MSXMidi::I8251Interf::setParityBit(bool enable, ParityBit parity)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.outConnector.setParityBit(enable, parity);
}

void MSXMidi::I8251Interf::recvByte(byte value, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.outConnector.recvByte(value, time);
}

void MSXMidi::I8251Interf::signal(EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, interf);
	midi.getPluggedMidiInDev().signal(time);
}


// Counter 0 output

void MSXMidi::Counter0::signal(ClockPin& pin, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, cntr0);
	ClockPin& clk = midi.i8251.getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXMidi::Counter0::signalPosEdge(ClockPin& /*pin*/, EmuTime::param /*time*/)
{
	UNREACHABLE;
}


// Counter 2 output

void MSXMidi::Counter2::signal(ClockPin& pin, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, cntr2);
	ClockPin& clk = midi.i8254.getClockPin(1);
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXMidi::Counter2::signalPosEdge(ClockPin& /*pin*/, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, cntr2);
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

void MSXMidi::recvByte(byte value, EmuTime::param time)
{
	i8251.recvByte(value, time);
}


template<typename Archive>
void MSXMidi::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);

	ar.template serializeBase<MidiInConnector>(*this);
	ar.serialize("outConnector", outConnector);

	ar.serialize("timerIRQ", timerIRQ);
	ar.serialize("rxrdyIRQ", rxrdyIRQ);
	ar.serialize("timerIRQlatch", timerIRQlatch);
	ar.serialize("timerIRQenabled", timerIRQenabled);
	ar.serialize("rxrdyIRQlatch", rxrdyIRQlatch);
	ar.serialize("rxrdyIRQenabled", rxrdyIRQenabled);
	ar.serialize("I8251", i8251);
	ar.serialize("I8254", i8254);
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
