// ATM this class does several things:
// - It connects the YMF278b chip to specific I/O ports in the MSX machine
// - It glues the YMF262 (FM-part) and YMF278 (Wave-part) classes together in a
//   full model of a YMF278b chip. IOW part of the logic of the YM278b is
//   modeled here instead of in a chip-specific class.
// TODO it would be nice to move the functionality of the 2nd point to a
//      different class, but until there's a 2nd user of this chip, this is
//      low priority.

#include "MSXMoonSound.hh"

#include "Clock.hh"
#include "serialize.hh"

#include "one_of.hh"
#include "unreachable.hh"

namespace openmsx {

// The master clock, running at 33.8MHz.
using MasterClock = Clock<33868800>;

// Required delay between register select and register read/write.
static constexpr auto FM_REG_SELECT_DELAY = MasterClock::duration(56);
// Required delay after register write.
static constexpr auto FM_REG_WRITE_DELAY  = MasterClock::duration(56);
// Datasheet doesn't mention any delay for reads from the FM registers. In fact
// it says reads from FM registers are not possible while tests on a real
// YMF278 show they do work (value of the NEW2 bit doesn't matter).

// Required delay between register select and register read/write.
static constexpr auto WAVE_REG_SELECT_DELAY = MasterClock::duration(88);
// Required delay after register write.
static constexpr auto WAVE_REG_WRITE_DELAY  = MasterClock::duration(88);
// Datasheet doesn't mention any delay for register reads (except for reads
// from register 6, see below). I also couldn't measure any delay on a real
// YMF278.

// Required delay after memory read.
static constexpr auto MEM_READ_DELAY  = MasterClock::duration(38);
// Required delay after memory write (instead of register write delay).
static constexpr auto MEM_WRITE_DELAY = MasterClock::duration(28);

// Required delay after instrument load.
// We pick 10000 cycles, this is approximately 300us (the number given in the
// datasheet). The exact number of cycles is unknown. But I did some (very
// rough) tests on real HW, and this number is not too bad (slightly too high
// but within 2%-4% of real value, needs more detailed tests).
static constexpr auto LOAD_DELAY = MasterClock::duration(10000);

static size_t getRamSize(const DeviceConfig& config)
{
	int ramSizeInKb = config.getChildDataAsInt("sampleram", 512);
	if (ramSizeInKb != one_of(
		   0,     //
		 128,     // 128kB
		 256,     // 128kB  128kB
		 512,     // 512kB
		 640,     // 512kB  128kB
		1024,     // 512kB  512kB
		2048)) {  // 512kB  512kB  512kB  512kB
		throw MSXException(
			"Wrong sample RAM size for Moonsound's YMF278. "
			"Got ", ramSizeInKb, ", but must be one of "
			"0, 128, 256, 512, 640, 1024 or 2048.");
	}
	return size_t(ramSizeInKb) * 1024; // kilo-bytes -> bytes
}

static void setupMemPtrs(bool mode0, std::span<const uint8_t> rom, std::span<const uint8_t> ram,
                         std::span<YMF278::Block128, 32> memPtrs)
{
	// /MCS0: connected to a 2MB ROM chip
	// For RAM there are multiple possibilities (but they all use /MCS6../MCS9)
	// * 128kB:
	//   1 SRAM chip of 128kB, chip enable (/CE) of this SRAM chip is connected to
	//   the 1Y0 output of a 74LS139 (2-to-4 decoder). The enable input of the
	//   74LS139 is connected to YMF278 pin /MCS6 and the 74LS139 1B:1A inputs are
	//   connected to YMF278 pins MA18:MA17. So the SRAM is selected when /MC6 is
	//   active and MA18:MA17 == 0:0.
	// * 256kB:
	//   2 SRAM chips of 128kB. First one connected as above. Second one has /CE
	//   connected to 74LS139 pin 1Y1. So SRAM2 is selected when /MSC6 is active
	//   and MA18:MA17 == 0:1.
	// * 512kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	// * 640kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	//   1 SRAM chip of 128kB, /CE connected to /MCS7.
	//     (This means SRAM2 is mirrored over a 512kB region)
	// * 1024kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	//   1 SRAM chip of 512kB, /CE connected to /MCS7
	// * 2048kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	//   1 SRAM chip of 512kB, /CE connected to /MCS7
	//   1 SRAM chip of 512kB, /CE connected to /MCS8
	//   1 SRAM chip of 512kB, /CE connected to /MCS9
	//   This configuration is not so easy to create on the v2.0 PCB. So it's
	//   very rare.
	// /MCS1../MCS5: unused
	static constexpr auto k128 = YMF278::k128;

	// first 2MB, ROM, both mode0 and mode1
	for (auto i : xrange(16)) {
		memPtrs[i] = subspan<k128>(rom, i * k128);
	}

	auto ramPart = [&](int i) {
		return (ram.size() >= (i + 1) * k128)
			? subspan<k128>(ram, i * k128)
			: YMF278::nullBlock;
	};
	if (mode0) [[likely]] {
		// second 2MB, RAM, as much as if available, upto 2MB
		for (auto i : xrange(16)) {
			memPtrs[i + 16] = ramPart(i);
		}
	} else {
		// mode1, normally this shouldn't be used on moonsound
		for (auto i : xrange(12)) {
			memPtrs[i + 16] = YMF278::nullBlock;
		}
		for (auto i : xrange(4)) {
			memPtrs[i + 28] = ramPart(i);
		}
	}
}

MSXMoonSound::MSXMoonSound(const DeviceConfig& config)
	: MSXDevice(config)
	, ymf262(getName() + " FM", config, true)
	, ymf278(getName() + " wave", getRamSize(config), config, setupMemPtrs)
	, ymf278LoadTime(getCurrentTime())
	, ymf278BusyTime(getCurrentTime())
{
	powerUp(getCurrentTime());
}

void MSXMoonSound::powerUp(EmuTime::param time)
{
	ymf278.clearRam();
	reset(time);
}

void MSXMoonSound::reset(EmuTime::param time)
{
	ymf262.reset(time);
	ymf278.reset(time);

	opl4latch = 0; // TODO check
	opl3latch = 0; // TODO check

	ymf278BusyTime = time;
	ymf278LoadTime = time;
}

byte MSXMoonSound::readIO(word port, EmuTime::param time)
{
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			return 255;
		case 1: // read wave register
			// Verified on real YMF278:
			// Even if NEW2=0 reads happen normally. Also reads
			// from sample memory (and thus the internal memory
			// pointer gets increased).
			if ((3 <= opl4latch) && (opl4latch <= 6)) {
				// This time is so small that on a MSX you can
				// never see BUSY=1. So I also couldn't test
				// whether this timing applies to registers 3-6
				// (like for write) or only to register 6. I
				// also couldn't test how the other registers
				// behave.
				// TODO Should we comment out this code? It
				// doesn't have any measurable effect on MSX.
				ymf278BusyTime = time + MEM_READ_DELAY;
			}
			return ymf278.readReg(opl4latch);
		default:
			UNREACHABLE;
		}
	} else {
		// FM part  0xC4-0xC7
		switch (port & 0x03) {
		case 0: // read status
		case 2:
			return ymf262.readStatus() | readYMF278Status(time);
		case 1:
		case 3: // read fm register
			return ymf262.readReg(opl3latch);
		default:
			UNREACHABLE;
		}
	}
}

byte MSXMoonSound::peekIO(word port, EmuTime::param time) const
{
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			return 255;
		case 1: // read wave register
			return ymf278.peekReg(opl4latch);
		default:
			UNREACHABLE;
		}
	} else {
		// FM part  0xC4-0xC7
		switch (port & 0x03) {
		case 0: // read status
		case 2:
			return ymf262.peekStatus() | readYMF278Status(time);
		case 1:
		case 3: // read fm register
			return ymf262.peekReg(opl3latch);
		default:
			UNREACHABLE;
		}
	}
}

void MSXMoonSound::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		if (getNew2()) {
			switch (port & 0x01) {
			case 0: // select register
				ymf278BusyTime = time + WAVE_REG_SELECT_DELAY;
				opl4latch = value;
				break;
			case 1:
				if ((0x08 <= opl4latch) && (opl4latch <= 0x1F)) {
					ymf278LoadTime = time + LOAD_DELAY;
				}
				if ((3 <= opl4latch) && (opl4latch <= 6)) {
					// Note: this time is so small that on
					// MSX you never see BUSY=1 for these
					// registers. Confirmed on real HW that
					// also registers 3-5 are faster.
					ymf278BusyTime = time + MEM_WRITE_DELAY;
				} else {
					// For the other registers it is
					// possible to see BUSY=1, but only
					// very briefly and only on R800.
					ymf278BusyTime = time + WAVE_REG_WRITE_DELAY;
				}
				if (opl4latch == 0xf8) {
					ymf262.setMixLevel(value, time);
				} else if (opl4latch == 0xf9) {
					ymf278.setMixLevel(value, time);
				}
				ymf278.writeReg(opl4latch, value, time);
				break;
			default:
				UNREACHABLE;
			}
		} else {
			// Verified on real YMF278:
			// Writes are ignored when NEW2=0 (both register select
			// and register write).
		}
	} else {
		// FM part  0xC4-0xC7
		switch (port & 0x03) {
		case 0: // select register bank 0
			opl3latch = value;
			ymf278BusyTime = time + FM_REG_SELECT_DELAY;
			break;
		case 2: // select register bank 1
			opl3latch = value | 0x100;
			ymf278BusyTime = time + FM_REG_SELECT_DELAY;
			break;
		case 1:
		case 3: // write fm register
			ymf278BusyTime = time + FM_REG_WRITE_DELAY;
			ymf262.writeReg(opl3latch, value, time);
			break;
		default:
			UNREACHABLE;
		}
	}
}

bool MSXMoonSound::getNew2() const
{
	return (ymf262.peekReg(0x105) & 0x02) != 0;
}

byte MSXMoonSound::readYMF278Status(EmuTime::param time) const
{
	byte result = 0;
	if (time < ymf278BusyTime) result |= 0x01;
	if (time < ymf278LoadTime) result |= 0x02;
	return result;
}

// version 1: initial version
// version 2: added alreadyReadID
// version 3: moved loadTime and busyTime from YMF278 to here
//            removed alreadyReadID
template<typename Archive>
void MSXMoonSound::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ymf262",    ymf262,
	             "ymf278",    ymf278,
	             "opl3latch", opl3latch,
	             "opl4latch", opl4latch);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("loadTime", ymf278LoadTime,
		             "busyTime", ymf278BusyTime);
	} else {
		assert(Archive::IS_LOADER);
		// For 100% backwards compatibility we should restore these two
		// from the (old) YMF278 class. Though that's a lot of extra
		// work for very little gain.
		ymf278LoadTime = getCurrentTime();
		ymf278BusyTime = getCurrentTime();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMoonSound);
REGISTER_MSXDEVICE(MSXMoonSound, "MoonSound");

} // namespace openmsx
