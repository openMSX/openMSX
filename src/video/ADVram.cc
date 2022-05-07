#include "ADVram.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <algorithm>

namespace openmsx {

ADVram::ADVram(const DeviceConfig& config)
	: MSXDevice(config)
	, vdp(nullptr)
	, vram(nullptr)
	, hasEnable(config.getChildDataAsBool("hasEnable", true))
{
	reset(EmuTime::dummy());
}

void ADVram::init()
{
	MSXDevice::init();

	const auto& refs = getReferences();
	if (refs.size() != 1) {
		throw MSXException("Invalid ADVRAM configuration: "
		                   "need reference to VDP device.");
	}
	vdp = dynamic_cast<VDP*>(refs[0]);
	if (!vdp) {
		throw MSXException("Invalid ADVRAM configuration: device '",
		                   refs[0]->getName(), "' is not a VDP device.");
	}
	vram = &vdp->getVRAM();
	mask = std::min(vram->getSize(), 128u * 1024) - 1;
}

void ADVram::reset(EmuTime::param /*time*/)
{
	// TODO figure out exactly what happens during reset
	baseAddr = 0;
	planar = false;
	enabled = !hasEnable;
}

byte ADVram::readIO(word port, EmuTime::param /*time*/)
{
	// ADVram only gets 'read's from 0x9A
	if (hasEnable) {
		enabled = ((port & 0x8000) != 0);
		planar  = ((port & 0x4000) != 0);
	} else {
		planar  = ((port & 0x0100) != 0);
	}
	return 0xFF;
}

void ADVram::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	// set mapper register
	baseAddr = (value & 0x07) << 14;
}

unsigned ADVram::calcAddress(word address) const
{
	unsigned addr = (address & 0x3FFF) | baseAddr;
	if (planar) {
		addr = ((addr & 1) << 16) | (addr >> 1);
	}
	return addr & mask;
}

byte ADVram::readMem(word address, EmuTime::param time)
{
	return enabled ? vram->cpuRead(calcAddress(address), time) : 0xFF;
}

void ADVram::writeMem(word address, byte value, EmuTime::param time)
{
	if (enabled) {
		vram->cpuWrite(calcAddress(address), value, time);
	}
}

void ADVram::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("baseAddr", baseAddr,
	             "enabled",  enabled,
	             "planar",   planar);
}
INSTANTIATE_SERIALIZE_METHODS(ADVram);
REGISTER_MSXDEVICE(ADVram, "ADVRAM");

} // namespace openmsx
