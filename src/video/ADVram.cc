// $Id$

#include "ADVram.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "MSXMotherBoard.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include <algorithm>

namespace openmsx {

ADVram::ADVram(MSXMotherBoard& motherBoard, const XMLElement& config,
               const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, vdp(NULL)
	, vram(NULL)
{
	hasEnable = config.getChildDataAsBool("hasEnable", true);

	const MSXDevice::Devices& references = getReferences();
	if (references.size() != 1) {
		throw MSXException("Invalid ADVRAM configuration: "
		                   "need reference to VDP device.");
	}
	vdp = dynamic_cast<VDP*>(references[0]);
	if (!vdp) {
		throw MSXException("Invalid ADVRAM configuration: device '" +
			references[0]->getName() + "' is not a VDP device.");
	}
	vram = &vdp->getVRAM();
	mask = std::min(vram->getSize(), 128u * 1024) - 1;
	
	reset(time);
}

void ADVram::reset(const EmuTime& /*time*/)
{
	// TODO figure out exactly what happens during reset
	baseAddr = 0;
	planar = false;
	enabled = !hasEnable;
}

byte ADVram::readIO(word port, const EmuTime& /*time*/)
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

void ADVram::writeIO(word /*port*/, byte value, const EmuTime& /*time*/)
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

byte ADVram::readMem(word address, const EmuTime& time)
{
	return enabled ? vram->cpuRead(calcAddress(address), time) : 0xFF;
}

void ADVram::writeMem(word address, byte value, const EmuTime& time)
{
	if (enabled) {
		vram->cpuWrite(calcAddress(address), value, time);
	}
}

} // namespace openmsx
