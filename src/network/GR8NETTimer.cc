#include "GR8NETTimer.hh"

#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

static constexpr uint16_t REG_COUNT_L = 0x5FEB;
static constexpr uint16_t REG_COUNT_M = 0x5FEC;
static constexpr uint16_t REG_COUNT_H = 0x5FED;

GR8NETTimer::GR8NETTimer(const DeviceConfig& config)
	: MSXDevice(config)
	, clock(getCurrentTime())
{
}

void GR8NETTimer::reset(EmuTime time)
{
	clock.reset(time);
	latch = 0;
}

// A 24-bit down counter. A full period (2^24) reads as 0.
uint32_t GR8NETTimer::getCounter(EmuTime time) const
{
	return uint32_t(-clock.getTicksTill(time)) & 0xFFFFFF;
}

byte GR8NETTimer::readMem(uint16_t address, EmuTime time)
{
	if (address == REG_COUNT_L) {
		latch = getCounter(time);
	}
	return peekMem(address, time);
}

byte GR8NETTimer::peekMem(uint16_t address, EmuTime time) const
{
	switch (address) {
	case REG_COUNT_L: return byte(getCounter(time));
	case REG_COUNT_M: return byte(latch >> 8);
	case REG_COUNT_H: return byte(latch >> 16);
	default:          return 0xFF;
	}
}

const byte* GR8NETTimer::getReadCacheLine(uint16_t start) const
{
	if (start == (REG_COUNT_L & CacheLine::HIGH)) return nullptr;
	return unmappedRead.data();
}

byte* GR8NETTimer::getWriteCacheLine(uint16_t /*start*/)
{
	return unmappedWrite.data();
}

template<typename Archive>
void GR8NETTimer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("clock", clock,
	             "latch", latch);
}
INSTANTIATE_SERIALIZE_METHODS(GR8NETTimer);
REGISTER_MSXDEVICE(GR8NETTimer, "GR8NETTimer");

} // namespace openmsx
