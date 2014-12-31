#include "MSXS1990.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"
#include "FirmwareSwitch.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "memory.hh"

namespace openmsx {

MSXS1990::MSXS1990(const DeviceConfig& config)
	: MSXDevice(config)
	, firmwareSwitch(make_unique<FirmwareSwitch>(config))
	, debuggable(getMotherBoard(), *this)
{
	reset(EmuTime::dummy());
}

MSXS1990::~MSXS1990()
{
}

void MSXS1990::reset(EmuTime::param /*time*/)
{
	registerSelect = 0; // TODO check this
	setCPUStatus(96);
}

byte MSXS1990::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXS1990::peekIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 0x01) {
	case 0:
		return registerSelect;
	case 1:
		return readRegister(registerSelect);
	default: // unreachable, avoid warning
		UNREACHABLE;
		return 0;
	}
}

void MSXS1990::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	switch (port & 0x01) {
	case 0:
		registerSelect = value;
		break;
	case 1:
		writeRegister(registerSelect, value);
		break;
	default:
		UNREACHABLE;
	}
}

byte MSXS1990::readRegister(byte reg) const
{
	switch (reg) {
	case 5:
		return firmwareSwitch->getStatus() ? 0x40 : 0x00;
	case 6:
		return cpuStatus;
	case 13:
		return 0x03; // TODO
	case 14:
		return 0x2F; // TODO
	case 15:
		return 0x8B; // TODO
	default:
		return 0xFF;
	}
}

void MSXS1990::writeRegister(byte reg, byte value)
{
	switch (reg) {
	case 6:
		setCPUStatus(value);
		break;
	}
}

void MSXS1990::setCPUStatus(byte value)
{
	cpuStatus = value & 0x60;
	getCPU().setActiveCPU((cpuStatus & 0x20) ? MSXCPU::CPU_Z80 :
	                                           MSXCPU::CPU_R800);
	bool dram = (cpuStatus & 0x40) ? false : true;
	getCPU().setDRAMmode(dram);
	getMotherBoard().getPanasonicMemory().setDRAM(dram);
	// TODO bit 7 -> reset MSX ?????
}


MSXS1990::Debuggable::Debuggable(MSXMotherBoard& motherBoard, MSXS1990& s1990_)
	: SimpleDebuggable(motherBoard, s1990_.getName() + " regs",
	                   "S1990 registers", 16)
	, s1990(s1990_)
{
}

byte MSXS1990::Debuggable::read(unsigned address)
{
	return s1990.readRegister(address);
}

void MSXS1990::Debuggable::write(unsigned address, byte value)
{
	s1990.writeRegister(address, value);
}


template<typename Archive>
void MSXS1990::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("registerSelect", registerSelect);
	ar.serialize("cpuStatus", cpuStatus);
	if (ar.isLoader()) {
		setCPUStatus(cpuStatus);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXS1990);
REGISTER_MSXDEVICE(MSXS1990, "S1990");

} // namespace openmsx
