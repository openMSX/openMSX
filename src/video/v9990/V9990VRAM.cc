#include "V9990.hh"
#include "V9990VRAM.hh"
#include "serialize.hh"
#include <cstring>

namespace openmsx {

V9990VRAM::V9990VRAM(V9990& vdp_, EmuTime::param /*time*/)
	: vdp(vdp_), cmdEngine(nullptr)
	, data(vdp.getDeviceConfig2(), vdp.getName() + " VRAM",
	       "V9990 Video RAM", VRAM_SIZE)
{
}

void V9990VRAM::clear()
{
	// Initialize memory. Alternate 0x00/0xff every 512 bytes.
	auto size = data.getSize();
	assert((size % 1024) == 0);
	auto* d = data.getWriteBackdoor();
	auto* e = d + size;
	while (d != e) {
		memset(d, 0x00, 512); d += 512;
		memset(d, 0xff, 512); d += 512;
	}
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

byte V9990VRAM::readVRAMCPU(unsigned address, EmuTime::param time)
{
	// note: used for both normal and debug read
	sync(time);
	return data[mapAddress(address)];
}

void V9990VRAM::writeVRAMCPU(unsigned address, byte value, EmuTime::param time)
{
	sync(time);
	data.write(mapAddress(address), value);
}

template<typename Archive>
void V9990VRAM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("data", data);
}
INSTANTIATE_SERIALIZE_METHODS(V9990VRAM);

} // namespace openmsx
