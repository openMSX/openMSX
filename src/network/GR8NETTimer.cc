#include "GR8NETTimer.hh"

#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

// Special control registers in bank 0
static constexpr uint16_t REG_CONTROL = 0x5FEA;
static constexpr uint16_t REG_COUNT_L = 0x5FEB;
static constexpr uint16_t REG_COUNT_M = 0x5FEC;
static constexpr uint16_t REG_COUNT_H = 0x5FED;

// Control register bits
static constexpr byte CTRL_OP  = 0x01;
static constexpr byte CTRL_RST = 0x02;
static constexpr byte CTRL_CLS = 0x20;
static constexpr byte CTRL_INT = 0x80;
static constexpr byte CTRL_STORED = 0x3D; // OP, HZP, WDE, IGE, CLS

static constexpr uint32_t COUNTER_RANGE = 1 << 24;

GR8NETTimer::GR8NETTimer(const DeviceConfig& config)
	: MSXDevice(config)
{
	reset(getCurrentTime());
}

void GR8NETTimer::reset(EmuTime time)
{
	control = 0;
	intFlag = false;
	latch = 0;
	writeLow = writeMid = 0;
	readMid = readHigh = 0;
	start = time;
	counter = period();
}

// The period a reload uses: the latched value, 0 meaning 2^24.
uint32_t GR8NETTimer::period() const
{
	return latch ? latch : COUNTER_RANGE;
}

// The ticks left in the current period at 'time', and whether a period
// ended since 'start'.
GR8NETTimer::State GR8NETTimer::calc(EmuTime time) const
{
	if (!(control & CTRL_OP)) return {counter, false};
	uint64_t freq = (control & CTRL_CLS) ? 3'579'545 : 10'000'000;
	// floor(d * freq / MAIN_FREQ), exactly, without overflow
	uint64_t d = (time - start).toUint64();
	uint64_t ticks = (d / MAIN_FREQ) * freq + (d % MAIN_FREQ) * freq / MAIN_FREQ;
	if (ticks < counter) return {uint32_t(counter - ticks), false};
	auto p = period();
	return {uint32_t(p - (ticks - counter) % p), true};
}

// Restart the bookkeeping at 'time'. Only writes do this: it drops the
// fraction of a tick that had elapsed.
void GR8NETTimer::sync(EmuTime time)
{
	auto [left, expired] = calc(time);
	counter = left;
	intFlag |= expired;
	start = time;
}

byte GR8NETTimer::readMem(uint16_t address, EmuTime time)
{
	if (address == REG_COUNT_L) {
		// Reading the LSB latches the MID and MSB for the reads that
		// follow. A full period (2^24) reads as 0.
		auto value = calc(time).left;
		readMid  = byte(value >> 8);
		readHigh = byte(value >> 16);
	}
	return peekMem(address, time);
}

byte GR8NETTimer::peekMem(uint16_t address, EmuTime time) const
{
	switch (address) {
	case REG_CONTROL: {
		// INT: a period ended since it was last cleared
		bool intNow = intFlag || calc(time).expired;
		return control | (intNow ? CTRL_INT : 0);
	}
	case REG_COUNT_L: return byte(calc(time).left);
	case REG_COUNT_M: return readMid;
	case REG_COUNT_H: return readHigh;
	default:          return 0xFF;
	}
}

void GR8NETTimer::writeMem(uint16_t address, byte value, EmuTime time)
{
	switch (address) {
	case REG_CONTROL:
		sync(time);
		// RST reloads the period and clears INT; writing 1 to INT
		// clears it. HZP, WDE and IGE are only stored.
		if (value & (CTRL_RST | CTRL_INT)) intFlag = false;
		if (value & CTRL_RST) counter = period();
		control = value & CTRL_STORED;
		break;
	case REG_COUNT_L:
		writeLow = value;
		break;
	case REG_COUNT_M:
		writeMid = value;
		break;
	case REG_COUNT_H:
		// Writing the MSB latches the whole 24-bit value. The current
		// period runs on; the following ones use the new value.
		sync(time);
		latch = writeLow | (writeMid << 8) | (value << 16);
		break;
	default:
		break;
	}
}

const byte* GR8NETTimer::getReadCacheLine(uint16_t start_) const
{
	if (start_ == (REG_CONTROL & CacheLine::HIGH)) return nullptr;
	return unmappedRead.data();
}

byte* GR8NETTimer::getWriteCacheLine(uint16_t start_)
{
	if (start_ == (REG_CONTROL & CacheLine::HIGH)) return nullptr;
	return unmappedWrite.data();
}

template<typename Archive>
void GR8NETTimer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("control",  control,
	             "intFlag",  intFlag,
	             "latch",    latch,
	             "writeLow", writeLow,
	             "writeMid", writeMid,
	             "readMid",  readMid,
	             "readHigh", readHigh,
	             "start",    start,
	             "counter",  counter);
}
INSTANTIATE_SERIALIZE_METHODS(GR8NETTimer);
REGISTER_MSXDEVICE(GR8NETTimer, "GR8NETTimer");

} // namespace openmsx
