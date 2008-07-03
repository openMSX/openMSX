// $Id$

#include "V9990.hh"
#include "V9990VRAM.hh"
#include "serialize.hh"
#include <cstring>

namespace openmsx {

V9990VRAM::V9990VRAM(V9990& vdp_, const EmuTime& /*time*/)
	: vdp(vdp_), cmdEngine(0)
	, data(vdp.getMotherBoard(), vdp.getName() + " VRAM",
	       "V9990 Video RAM", VRAM_SIZE)
{
	memset(&data[0], 0, data.getSize());
}

unsigned V9990VRAM::mapAddress(unsigned address)
{
	address &= 0x7FFFF; // change to assert?
	switch (vdp.getDisplayMode()) {
		case P1:
			return transformP1(address);
		case P2:
			return transformP2(address);
		default /* Bx */:
			return transformBx(address);
	}
}

byte V9990VRAM::readVRAMCPU(unsigned address, const EmuTime& time)
{
	sync(time);
	return data[mapAddress(address)];
}

void V9990VRAM::writeVRAMCPU(unsigned address, byte value, const EmuTime& time)
{
	sync(time);
	data[mapAddress(address)] = value;
}

void V9990VRAM::setCmdEngine(V9990CmdEngine& cmdEngine_)
{
	cmdEngine = &cmdEngine_;
}


template<typename Archive>
void V9990VRAM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("data", data);
}
INSTANTIATE_SERIALIZE_METHODS(V9990VRAM);

} // namespace openmsx
