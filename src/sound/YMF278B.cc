#include "YMF278B.hh"

#include "Clock.hh"
#include "serialize.hh"

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

YMF278B::YMF278B(const std::string& name, size_t ramSize, const DeviceConfig& config,
                 YMF278::SetupMemPtrFunc setupMemPtrs, EmuTime::param time)
	: ymf262(name + " FM", config, true)
	, ymf278(name + " wave", ramSize, config, std::move(setupMemPtrs))
	, ymf278LoadTime(time)
	, ymf278BusyTime(time)
{
	powerUp(time);
}

void YMF278B::powerUp(EmuTime::param time)
{
	ymf278.clearRam();
	reset(time);
}

void YMF278B::reset(EmuTime::param time)
{
	ymf262.reset(time);
	ymf278.reset(time);

	opl4latch = 0; // TODO check
	opl3latch = 0; // TODO check

	ymf278BusyTime = time;
	ymf278LoadTime = time;
}

byte YMF278B::readIO(word port, EmuTime::param time)
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

byte YMF278B::peekIO(word port, EmuTime::param time) const
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

void YMF278B::writeIO(word port, byte value, EmuTime::param time)
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

bool YMF278B::getNew2() const
{
	return (ymf262.peekReg(0x105) & 0x02) != 0;
}

byte YMF278B::readYMF278Status(EmuTime::param time) const
{
	byte result = 0;
	if (time < ymf278BusyTime) result |= 0x01;
	if (time < ymf278LoadTime) result |= 0x02;
	return result;
}

void YMF278B::setupMemoryPointers()
{
	ymf278.setupMemoryPointers();
}

// For backwards compatible savestates with (old) MSXMoonSound class.
// version 1: initial version
// version 2: added alreadyReadID
// version 3: moved loadTime and busyTime from YMF278 to here
//            removed alreadyReadID
void YMF278B::serialize_bw_compat(XmlInputArchive& ar, unsigned version, EmuTime::param time)
{
	ar.serialize("ymf262",    ymf262,
	             "ymf278",    ymf278,
	             "opl3latch", opl3latch,
	             "opl4latch", opl4latch);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("loadTime", ymf278LoadTime,
		             "busyTime", ymf278BusyTime);
	} else {
		assert(XmlInputArchive::IS_LOADER);
		// For 100% backwards compatibility we should restore these two
		// from the (old) YMF278 class. Though that's a lot of extra
		// work for very little gain.
		ymf278LoadTime = time;
		ymf278BusyTime = time;
	}
}

// version 1: initial version
template<typename Archive>
void YMF278B::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("ymf262",    ymf262,
	             "ymf278",    ymf278,
	             "opl3latch", opl3latch,
	             "opl4latch", opl4latch,
	             "loadTime",  ymf278LoadTime,
	             "busyTime",  ymf278BusyTime);
}
INSTANTIATE_SERIALIZE_METHODS(YMF278B);

} // namespace openmsx
