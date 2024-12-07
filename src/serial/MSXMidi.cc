#include "MSXMidi.hh"
#include "MidiInDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "outer.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

// Documented in MSX-Datapack Vol. 3, section 4 (MSX-MIDI), from page 634
static constexpr byte LIMITED_RANGE_VALUE = 0x01; // b0 = "E8" => determines port range
static constexpr byte DISABLED_VALUE      = 0x80; // b7 = EN

MSXMidi::MSXMidi(const DeviceConfig& config)
	: MSXDevice(config)
	, MidiInConnector(MSXDevice::getPluggingController(), "MSX-MIDI-in")
	, timerIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQtimer")
	, rxrdyIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQrxrdy")
	, isExternalMSXMIDI(config.findChild("external") != nullptr)
	, isEnabled(!isExternalMSXMIDI)
	, outConnector(MSXDevice::getPluggingController(), "MSX-MIDI-out")
	, i8251(getScheduler(), interface, getCurrentTime())
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
			UNREACHABLE;
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
			UNREACHABLE;
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

	auto& cpuInterface = getCPUInterface();
	if (newIsEnabled != isEnabled) {
		// Enable/disabled status changes, possibly limited status
		// changes as well but that doesn't matter, we anyway need
		// to (un)register the full port range.
		if (newIsEnabled) {
			// disabled -> enabled
			if (newIsLimited) {
				cpuInterface.register_IO_InOut_range(0xE0, 2, this);
			} else {
				cpuInterface.register_IO_InOut_range(0xE8, 8, this);
			}
		} else {
			// enabled -> disabled
			if (isLimitedTo8251) { // note: old isLimited status
				cpuInterface.unregister_IO_InOut_range(0xE0, 2, this);
			} else {
				cpuInterface.unregister_IO_InOut_range(0xE8, 8, this);
			}
		}

	} else if (isEnabled && (newIsLimited != isLimitedTo8251)) {
		// Remains enabled, and only 'isLimited' status changes.
		// Need to switch between the low/high range.
		if (newIsLimited) {
			// Switch high->low range.
			cpuInterface.unregister_IO_InOut_range(0xE8, 8, this);
			cpuInterface.register_IO_InOut_range  (0xE0, 2, this);
		} else {
			// Switch low->high range.
			cpuInterface.unregister_IO_InOut_range(0xE0, 2, this);
			cpuInterface.register_IO_InOut_range  (0xE8, 8, this);
		}
	}

	isEnabled       = newIsEnabled;
	isLimitedTo8251 = newIsLimited;
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

void MSXMidi::Interface::setRxRDY(bool status, EmuTime::param /*time*/)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.setRxRDYIRQ(status);
}

void MSXMidi::Interface::setDTR(bool status, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.enableTimerIRQ(status, time);
}

void MSXMidi::Interface::setRTS(bool status, EmuTime::param /*time*/)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.enableRxRDYIRQ(status);
}

bool MSXMidi::Interface::getDSR(EmuTime::param /*time*/)
{
	const auto& midi = OUTER(MSXMidi, interface);
	return midi.timerIRQ.getState();
}

bool MSXMidi::Interface::getCTS(EmuTime::param /*time*/)
{
	return true;
}

void MSXMidi::Interface::setDataBits(DataBits bits)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.outConnector.setDataBits(bits);
}

void MSXMidi::Interface::setStopBits(StopBits bits)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.outConnector.setStopBits(bits);
}

void MSXMidi::Interface::setParityBit(bool enable, Parity parity)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.outConnector.setParityBit(enable, parity);
}

void MSXMidi::Interface::recvByte(byte value, EmuTime::param time)
{
	auto& midi = OUTER(MSXMidi, interface);
	midi.outConnector.recvByte(value, time);
}

void MSXMidi::Interface::signal(EmuTime::param time)
{
	const auto& midi = OUTER(MSXMidi, interface);
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

void MSXMidi::setParityBit(bool enable, Parity parity)
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
	ar.serialize("outConnector",    outConnector,
	             "timerIRQ",        timerIRQ,
	             "rxrdyIRQ",        rxrdyIRQ,
	             "timerIRQlatch",   timerIRQlatch,
	             "timerIRQenabled", timerIRQenabled,
	             "rxrdyIRQlatch",   rxrdyIRQlatch,
	             "rxrdyIRQenabled", rxrdyIRQenabled,
	             "I8251",           i8251,
	             "I8254",           i8254);
	if (ar.versionAtLeast(version, 2)) {
		bool newIsEnabled = isEnabled; // copy for saver
		bool newIsLimitedTo8251 = isLimitedTo8251; // copy for saver
		ar.serialize("isEnabled",       newIsEnabled,
		             "isLimitedTo8251", newIsLimitedTo8251);
		if constexpr (Archive::IS_LOADER) {
			if (isExternalMSXMIDI) {
				registerIOports((newIsEnabled ? 0x00 : DISABLED_VALUE) | (newIsLimitedTo8251 ? LIMITED_RANGE_VALUE : 0x00));
			}
		}
	}

	// don't serialize:  cntr0, cntr2, interface
}
INSTANTIATE_SERIALIZE_METHODS(MSXMidi);
REGISTER_MSXDEVICE(MSXMidi, "MSX-Midi");

} // namespace openmsx
