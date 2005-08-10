// $Id$

/** V9990 VRAM
  *
  * Video RAM for the V9990
  */

#include "V9990.hh"
#include "V9990VRAM.hh"

namespace openmsx {

V9990VRAM::V9990VRAM(V9990& vdp_, const EmuTime& /*time*/)
	: vdp(vdp_)
	, data(vdp.getMotherBoard(), "V9990 VRAM", "V9990 Video RAM", VRAM_SIZE)
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

byte V9990VRAM::readVRAMSlow(unsigned address)
{
	return data[mapAddress(address)];
}

void V9990VRAM::writeVRAMSlow(unsigned address, byte value)
{
	data[mapAddress(address)] = value;
}

} // namespace openmsx
