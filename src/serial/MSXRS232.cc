#include "MSXRS232.hh"
#include "RS232Device.hh"
#include "CacheLine.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "BooleanSetting.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "one_of.hh"
#include "outer.hh"
#include "unreachable.hh"
#include <cassert>
#include <memory>

namespace openmsx {

const unsigned RAM_OFFSET = 0x2000;
const unsigned RAM_SIZE = 0x800;

MSXRS232::MSXRS232(const DeviceConfig& config)
	: MSXDevice(config)
	, RS232Connector(MSXDevice::getPluggingController(), "msx-rs232")
	, i8254(getScheduler(), &cntr0, &cntr1, nullptr, getCurrentTime())
	, i8251(getScheduler(), interf, getCurrentTime())
	, rom(config.findChild("rom")
		? std::make_unique<Rom>(
			MSXDevice::getName() + " ROM", "rom", config)
		: nullptr) // when the ROM is already mapped, you don't want to specify it again here
	, ram(config.getChildDataAsBool("ram", false)
		? std::make_unique<Ram>(
			config, MSXDevice::getName() + " RAM",
	                "RS232 RAM", RAM_SIZE)
		: nullptr)
	, rxrdyIRQ(getMotherBoard(), MSXDevice::getName() + ".IRQrxrdy")
	, rxrdyIRQlatch(false)
	, rxrdyIRQenabled(false)
	, hasMemoryBasedIo(config.getChildDataAsBool("memorybasedio", false))
	, ioAccessEnabled(!hasMemoryBasedIo)
	, switchSetting(config.getChildDataAsBool("toshiba_rs232c_switch",
		false) ? std::make_unique<BooleanSetting>(getCommandController(),
		"toshiba_rs232c_switch", "status of the RS-232C enable switch",
		true) : nullptr)
{
	if (rom && (rom->getSize() != one_of(0x2000u, 0x4000u))) {
		throw MSXException("RS232C only supports 8kB or 16kB ROMs.");
	}

	EmuDuration total(1.0 / 1.8432e6); // 1.8432MHz
	EmuDuration hi   (1.0 / 3.6864e6); //   half clock period
	EmuTime::param time = getCurrentTime();
	i8254.getClockPin(0).setPeriodicState(total, hi, time);
	i8254.getClockPin(1).setPeriodicState(total, hi, time);
	i8254.getClockPin(2).setPeriodicState(total, hi, time);

	powerUp(time);
}

MSXRS232::~MSXRS232() = default;

void MSXRS232::powerUp(EmuTime::param time)
{
	if (ram) ram->clear();
	reset(time);
}

void MSXRS232::reset(EmuTime::param /*time*/)
{
	rxrdyIRQlatch = false;
	rxrdyIRQenabled = false;
	rxrdyIRQ.reset();

	ioAccessEnabled = !hasMemoryBasedIo;

	if (ram) ram->clear();
}

byte MSXRS232::readMem(word address, EmuTime::param time)
{
	if (hasMemoryBasedIo && (0xBFF8 <= address) && (address <= 0xBFFF)) {
		return readIOImpl(address & 0x07, time);
	}
	word addr = address & 0x3FFF;
	if (ram && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		return (*ram)[addr - RAM_OFFSET];
	} else if (rom && (0x4000 <= address) && (address < 0x8000)) {
		return (*rom)[addr & (rom->getSize() - 1)];
	} else {
		return 0xFF;
	}
}

const byte* MSXRS232::getReadCacheLine(word start) const
{
	if (hasMemoryBasedIo && (start == (0xBFF8 & CacheLine::HIGH))) {
		return nullptr;
	}
	word addr = start & 0x3FFF;
	if (ram && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		return &(*ram)[addr - RAM_OFFSET];
	} else if (rom && (0x4000 <= start) && (start < 0x8000)) {
		return &(*rom)[addr & (rom->getSize() - 1)];
	} else {
		return unmappedRead;
	}
}

void MSXRS232::writeMem(word address, byte value, EmuTime::param time)
{

	if (hasMemoryBasedIo && (0xBFF8 <= address) && (address <= 0xBFFF)) {
		// when the interface has memory based I/O, the I/O port
		// based I/O is disabled, but it can be enabled by writing
		// bit 4 to 0xBFFA. It is disabled again at reset.
		// Source: Sony HB-G900P and Sony HB-G900AP service manuals.
		// We assume here you can also disable it by writing 0 to it.
		if (address == 0xBFFA) {
			ioAccessEnabled = (value & (1 << 4))!=0;
		}
		return writeIOImpl(address & 0x07, value, time);
	}
	word addr = address & 0x3FFF;
	if (ram && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		(*ram)[addr - RAM_OFFSET] = value;
	}
}

byte* MSXRS232::getWriteCacheLine(word start) const
{
	if (hasMemoryBasedIo && (start == (0xBFF8 & CacheLine::HIGH))) {
		return nullptr;
	}
	word addr = start & 0x3FFF;
	if (ram && ((RAM_OFFSET <= addr) && (addr < (RAM_OFFSET + RAM_SIZE)))) {
		return &(*ram)[addr - RAM_OFFSET];
	} else {
		return unmappedWrite;
	}
}

bool MSXRS232::allowUnaligned() const
{
	// OK, because this device doesn't call any 'fillDeviceXXXCache()'functions.
	return true;
}

byte MSXRS232::readIO(word port, EmuTime::param time)
{
	if (ioAccessEnabled) {
		return readIOImpl(port & 0x07, time);
	}
	return 0xFF;
}

byte MSXRS232::readIOImpl(word port, EmuTime::param time)
{
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			return i8251.readIO(port, time);
		case 2: // Status sense port
			return readStatus(time);
		case 3: // no function
			return 0xFF;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			return i8254.readIO(port - 4, time);
		default:
			UNREACHABLE; return 0;
	}
}

byte MSXRS232::peekIO(word port, EmuTime::param time) const
{
	if (hasMemoryBasedIo && !ioAccessEnabled) return 0xFF;
	port &= 0x07;
	switch (port) {
		case 0: // UART data register
		case 1: // UART status register
			return i8251.peekIO(port, time);
		case 2: // Status sense port
			return 0; // TODO not implemented
		case 3: // no function
			return 0xFF;
		case 4: // counter 0 data port
		case 5: // counter 1 data port
		case 6: // counter 2 data port
		case 7: // timer command register
			return i8254.peekIO(port - 4, time);
		default:
			UNREACHABLE; return 0;
	}
}

void MSXRS232::writeIO(word port, byte value, EmuTime::param time)
{
	if (ioAccessEnabled) writeIOImpl(port & 0x07, value, time);
}

void MSXRS232::writeIOImpl(word port, byte value, EmuTime::param time)
{
	switch (port) {
		case 0: // UART data register
		case 1: // UART command register
			i8251.writeIO(port, value, time);
			break;
		case 2: // interrupt mask register
			setIRQMask(value);
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

byte MSXRS232::readStatus(EmuTime::param time)
{

	// Info from http://nocash.emubase.de/portar.htm
	//
	//  Bit Name  Expl.
	//  0   CD    Carrier Detect   (0=Active, 1=Not active)
	//  1   RI    Ring Indicator   (0=Active, 1=Not active) (N/C in MSX)
	//  6         Timer Output from i8253 Counter 2
	//  7   CTS   Clear to Send    (0=Active, 1=Not active)
	//
	// On Toshiba HX-22, see
	//   http://www.msx.org/forum/msx-talk/hardware/toshiba-hx-22?page=3
	//   RetroTechie's post of 20-09-2012, 08:08
	//   ... The "RS-232 interrupt disable" bit can be read back via bit 3
	//   on this I/O port, if CN1 is open. If CN1 is closed, it always
	//   reads back as "0". ...

	byte result = 0; // TODO check unused bits

	// TODO bit 0: carrier detect

	if (!rxrdyIRQenabled && switchSetting && switchSetting->getBoolean()) {
		result |= 0x08;
	}

	if (!interf.getCTS(time)) {
		result |= 0x80;
	}
	if (i8254.getOutputPin(2).getState(time)) {
		result |= 0x40;
	}
	return result;
}

void MSXRS232::setIRQMask(byte value)
{
	enableRxRDYIRQ(!(value & 1));
}

void MSXRS232::setRxRDYIRQ(bool status)
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

void MSXRS232::enableRxRDYIRQ(bool enabled)
{
	if (rxrdyIRQenabled != enabled) {
		rxrdyIRQenabled = enabled;
		if (!rxrdyIRQenabled && rxrdyIRQlatch) {
			rxrdyIRQ.reset();
		}
	}
}


// I8251Interface  (pass calls from I8251 to outConnector)

void MSXRS232::I8251Interf::setRxRDY(bool status, EmuTime::param /*time*/)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.setRxRDYIRQ(status);
}

void MSXRS232::I8251Interf::setDTR(bool status, EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().setDTR(status, time);
}

void MSXRS232::I8251Interf::setRTS(bool status, EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().setRTS(status, time);
}

bool MSXRS232::I8251Interf::getDSR(EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	return rs232.getPluggedRS232Dev().getDSR(time);
}

bool MSXRS232::I8251Interf::getCTS(EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	return rs232.getPluggedRS232Dev().getCTS(time);
}

void MSXRS232::I8251Interf::setDataBits(DataBits bits)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().setDataBits(bits);
}

void MSXRS232::I8251Interf::setStopBits(StopBits bits)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().setStopBits(bits);
}

void MSXRS232::I8251Interf::setParityBit(bool enable, ParityBit parity)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().setParityBit(enable, parity);
}

void MSXRS232::I8251Interf::recvByte(byte value, EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().recvByte(value, time);
}

void MSXRS232::I8251Interf::signal(EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, interf);
	rs232.getPluggedRS232Dev().signal(time); // for input
}


// Counter 0 output

void MSXRS232::Counter0::signal(ClockPin& pin, EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, cntr0);
	ClockPin& clk = rs232.i8251.getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXRS232::Counter0::signalPosEdge(ClockPin& /*pin*/, EmuTime::param /*time*/)
{
	UNREACHABLE;
}


// Counter 1 output // TODO split rx tx

void MSXRS232::Counter1::signal(ClockPin& pin, EmuTime::param time)
{
	auto& rs232 = OUTER(MSXRS232, cntr1);
	ClockPin& clk = rs232.i8251.getClockPin();
	if (pin.isPeriodic()) {
		clk.setPeriodicState(pin.getTotalDuration(),
		                     pin.getHighDuration(), time);
	} else {
		clk.setState(pin.getState(time), time);
	}
}

void MSXRS232::Counter1::signalPosEdge(ClockPin& /*pin*/, EmuTime::param /*time*/)
{
	UNREACHABLE;
}


// RS232Connector input

bool MSXRS232::ready()
{
	return i8251.isRecvReady();
}

bool MSXRS232::acceptsData()
{
	return i8251.isRecvEnabled();
}

void MSXRS232::setDataBits(DataBits bits)
{
	i8251.setDataBits(bits);
}

void MSXRS232::setStopBits(StopBits bits)
{
	i8251.setStopBits(bits);
}

void MSXRS232::setParityBit(bool enable, ParityBit parity)
{
	i8251.setParityBit(enable, parity);
}

void MSXRS232::recvByte(byte value, EmuTime::param time)
{
	i8251.recvByte(value, time);
}

// version 1: initial version
// version 2: added ioAccessEnabled
// TODO: serialize switch status?
template<typename Archive>
void MSXRS232::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<RS232Connector>(*this);

	ar.serialize("I8254", i8254,
	             "I8251", i8251);
	if (ram) ar.serialize("ram", *ram);
	ar.serialize("rxrdyIRQ",        rxrdyIRQ,
	             "rxrdyIRQlatch",   rxrdyIRQlatch,
	             "rxrdyIRQenabled", rxrdyIRQenabled);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("ioAccessEnabled", ioAccessEnabled);
	} else {
		assert(Archive::IS_LOADER);
		ioAccessEnabled = !hasMemoryBasedIo; // we can't know the
					// actual value, but this is probably
					// safest
	}

	// don't serialize cntr0, cntr1, interf
}
INSTANTIATE_SERIALIZE_METHODS(MSXRS232);
REGISTER_MSXDEVICE(MSXRS232, "RS232");

} // namespace openmsx
