#include "V9990.hh"

#include "V9990VRAM.hh"

#include "serialize.hh"

#include <algorithm>

namespace openmsx {

V9990VRAM::V9990VRAM(V9990& vdp_, EmuTime::param /*time*/)
	: vdp(vdp_)
	, data(vdp.getDeviceConfig2(), vdp.getName() + " VRAM",
	       "V9990 Video RAM", VRAM_SIZE)
{
}

void V9990VRAM::clear()
{
	// Initialize memory. Alternate 0x00/0xff every 512 bytes.
	std::span s = data.getWriteBackdoor();
	assert((s.size() % 1024) == 0);
	while (!s.empty()) {
		std::ranges::fill(s.subspan(  0, 512), 0x00);
		std::ranges::fill(s.subspan(512, 512), 0xff);
		s = s.subspan(1024);
	}
}

unsigned V9990VRAM::mapAddress(unsigned address) const
{
	address &= 0x7FFFF; // change to assert?
	switch (vdp.getDisplayMode()) {
	case V9990DisplayMode::P1:
		return transformP1(address);
	case V9990DisplayMode::P2:
		return transformP2(address);
	default /* Bx */:
		return transformBx(address);
	}
}

uint8_t V9990VRAM::readVRAMCPU(unsigned address, EmuTime::param time)
{
	// note: used for both normal and debug read
	sync(time);
	return data[mapAddress(address)];
}

void V9990VRAM::writeVRAMCPU(unsigned address, uint8_t value, EmuTime::param time)
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
