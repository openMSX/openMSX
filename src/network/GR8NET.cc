#include "GR8NET.hh"

#include "CacheLine.hh"
#include "serialize.hh"

#include <array>

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
static constexpr byte CTRL_ER  = 0x40;
static constexpr byte CTRL_INT = 0x80;
static constexpr byte CTRL_STORED = 0x3C; // HZP, WDE, IGE, CLS

static constexpr uint32_t COUNTER_RANGE = 1 << 24;

// This adapter's ID (the configuration switches): always #0.
static constexpr byte ADAPTER_ID = 0;

GR8NET::GR8NET(const DeviceConfig& config)
	: MSXDevice(config)
{
	reset(getCurrentTime());
}

void GR8NET::reset(EmuTime time)
{
	port5E = 0;
	idIndex = 0;

	control = 0;
	running = false;
	intFlag = false;
	errFlag = false;
	latch = 0;
	writeLow = writeMid = 0;
	readMid = readHigh = 0;

	start = time;
	offset = 0;
	period = latchedPeriod();
	freq = 10'000'000;
	seenExpiries = 0;
}

// The period a reload uses: the latched value, 0 meaning 2^24.
uint32_t GR8NET::latchedPeriod() const
{
	return latch ? latch : COUNTER_RANGE;
}

GR8NET::Position GR8NET::position(EmuTime time) const
{
	uint64_t ticks = 0;
	if (running) {
		// floor(d * freq / MAIN_FREQ), exactly, without overflow
		uint64_t d = (time - start).toUint64();
		ticks = (d / MAIN_FREQ) * freq + (d % MAIN_FREQ) * freq / MAIN_FREQ;
	}
	uint64_t elapsed = offset + ticks;
	Position pos;
	if (elapsed < period) {
		pos.inPeriod = uint32_t(elapsed);
		pos.period = period;
		pos.expiries = 0;
	} else {
		uint32_t next = latchedPeriod();
		uint64_t after = elapsed - period;
		pos.inPeriod = uint32_t(after % next);
		pos.period = next;
		pos.expiries = 1 + after / next;
	}
	// The counter counts down from the period's length; 2^24 reads as 0.
	pos.value = (pos.period - pos.inPeriod) & (COUNTER_RANGE - 1);
	return pos;
}

// Set INT for the periods that ended up to 'time'.
void GR8NET::sync(EmuTime time)
{
	auto pos = position(time);
	if (pos.expiries > seenExpiries) {
		intFlag = true;
		seenExpiries = pos.expiries;
	}
}

// Restart the bookkeeping at 'time', keeping the counter where it is
// (after sync()).
void GR8NET::rebase(EmuTime time)
{
	auto pos = position(time);
	start = time;
	offset = pos.inPeriod;
	period = pos.period;
	seenExpiries = 0;
}

void GR8NET::writeControl(byte value, EmuTime time)
{
	sync(time);
	if (value & CTRL_INT) intFlag = false;
	if (value & CTRL_ER)  errFlag = false;
	control = value & CTRL_STORED;
	uint32_t newFreq = (control & CTRL_CLS) ? 3'579'545 : 10'000'000;

	bool op = value & CTRL_OP;
	if (value & CTRL_RST) {
		// Reset: reload the latched value, clear INT and ER, and count
		// (or not) as OP says.
		start = time;
		offset = 0;
		period = latchedPeriod();
		freq = newFreq;
		seenExpiries = 0;
		intFlag = false;
		errFlag = false;
		running = op;
	} else if (op && !running) {
		// resume
		start = time;
		freq = newFreq;
		seenExpiries = 0;
		running = true;
	} else if (!op && running) {
		// suspend
		rebase(time);
		running = false;
	}
}

byte GR8NET::readControl(EmuTime time) const
{
	bool intNow = intFlag || (position(time).expiries > seenExpiries);
	return (running ? CTRL_OP : 0) | control |
	       (errFlag ? CTRL_ER : 0) | (intNow ? CTRL_INT : 0);
}

byte GR8NET::readMem(uint16_t address, EmuTime time)
{
	switch (address) {
	case REG_CONTROL:
		sync(time);
		return readControl(time);
	case REG_COUNT_L: {
		// Reading the LSB latches the whole value for the MID and
		// MSB reads that follow.
		auto value = position(time).value;
		readMid  = byte(value >> 8);
		readHigh = byte(value >> 16);
		return byte(value);
	}
	default:
		return peekMem(address, time);
	}
}

byte GR8NET::peekMem(uint16_t address, EmuTime time) const
{
	switch (address) {
	case REG_CONTROL: return readControl(time);
	case REG_COUNT_L: return byte(position(time).value);
	case REG_COUNT_M: return readMid;
	case REG_COUNT_H: return readHigh;
	default:          return 0xFF;
	}
}

void GR8NET::writeMem(uint16_t address, byte value, EmuTime time)
{
	switch (address) {
	case REG_CONTROL:
		writeControl(value, time);
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
		rebase(time);
		latch = writeLow | (writeMid << 8) | (value << 16);
		break;
	default:
		break;
	}
}

const byte* GR8NET::getReadCacheLine(uint16_t start_) const
{
	if (start_ == (REG_CONTROL & CacheLine::HIGH)) return nullptr;
	return unmappedRead.data();
}

byte* GR8NET::getWriteCacheLine(uint16_t start_)
{
	if (start_ == (REG_CONTROL & CacheLine::HIGH)) return nullptr;
	return unmappedWrite.data();
}

// Port 5Fh, register 0: "GR8N", then the engine's build date (year as a
// little-endian word, month, day), in a loop.
static constexpr std::array<byte, 8> ID_SEQUENCE = {
	'G', 'R', '8', 'N', 2023 & 0xFF, 2023 >> 8, 3, 19
};

byte GR8NET::readPort5F() const
{
	if ((port5E >> 6) != ADAPTER_ID) return 0xFF;
	switch (port5E & 0x0F) {
	case 0:
		return ID_SEQUENCE[idIndex];
	case 1: {
		// slot ID in RDSLT format
		byte slotId = getPrimarySlot();
		if (int sec = getSecondarySlot(); sec != -1) {
			slotId |= byte(0x80 | (sec << 2));
		}
		return slotId;
	}
	default:
		return 0xFF;
	}
}

byte GR8NET::readIO(uint16_t port, EmuTime time)
{
	byte result = peekIO(port, time);
	if (((port & 0xFF) == 0x5F) && ((port5E >> 6) == ADAPTER_ID) &&
	    ((port5E & 0x0F) == 0)) {
		idIndex = (idIndex + 1) % ID_SEQUENCE.size();
	}
	return result;
}

byte GR8NET::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0xFF) {
	case 0x5E:
		return ((port5E >> 6) == ADAPTER_ID) ? port5E : 0xFF;
	case 0x5F:
		return readPort5F();
	default:
		return 0xFF;
	}
}

void GR8NET::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	if ((port & 0xFF) == 0x5E) {
		port5E = value;
		if ((value & 0x0F) == 0) idIndex = 0;
	}
	// port 5Fh: the writable registers aren't emulated
}

template<typename Archive>
void GR8NET::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("port5E",       port5E,
	             "idIndex",      idIndex,
	             "control",      control,
	             "running",      running,
	             "intFlag",      intFlag,
	             "errFlag",      errFlag,
	             "latch",        latch,
	             "writeLow",     writeLow,
	             "writeMid",     writeMid,
	             "readMid",      readMid,
	             "readHigh",     readHigh,
	             "start",        start,
	             "offset",       offset,
	             "period",       period,
	             "freq",         freq,
	             "seenExpiries", seenExpiries);
}
INSTANTIATE_SERIALIZE_METHODS(GR8NET);
REGISTER_MSXDEVICE(GR8NET, "GR8NET");

} // namespace openmsx
