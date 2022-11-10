// The actual sample playing part is duplicated for the 'emu' domain and the
// 'audio' domain. The emu part is responsible for cycle accurate sample
// read back (see peekReg() register 0x13 and 0x14) and for cycle accurate
// status register updates (the status bits related to playback, e.g.
// end-of-sample). The audio part is responsible for the actual sound
// generation. This split up allows for the two parts to be out-of-sync. So for
// example when emulation is running faster or slower than 100% realtime speed,
// we both get cycle accurate emulation behaviour and still sound generation at
// 100% realtime speed (which is most of the time better for sound quality).

#include "Y8950Adpcm.hh"
#include "Y8950.hh"
#include "Clock.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "Math.hh"
#include "narrow.hh"
#include "serialize.hh"
#include <algorithm>
#include <array>

namespace openmsx {

// Bitmask for register 0x07
static constexpr int R07_RESET       = 0x01;
static constexpr int R07_SP_OFF      = 0x08;
static constexpr int R07_REPEAT      = 0x10;
static constexpr int R07_MEMORY_DATA = 0x20;
static constexpr int R07_REC         = 0x40;
static constexpr int R07_START       = 0x80;
static constexpr int R07_MODE        = 0xE0;

// Bitmask for register 0x08
static constexpr int R08_ROM         = 0x01;
static constexpr int R08_64K         = 0x02;
static constexpr int R08_DA_AD       = 0x04;
static constexpr int R08_SAMPL       = 0x08;
static constexpr int R08_NOTE_SET    = 0x40;
static constexpr int R08_CSM         = 0x80;

static constexpr int DIFF_MAX     = 0x6000;
static constexpr int DIFF_MIN     = 0x7F;
static constexpr int DIFF_DEFAULT = 0x7F;

static constexpr int STEP_BITS = 16;
static constexpr int STEP_MASK = (1 << STEP_BITS) -1;


Y8950Adpcm::Y8950Adpcm(Y8950& y8950_, const DeviceConfig& config,
                       const std::string& name, unsigned sampleRam)
	: Schedulable(config.getScheduler())
	, y8950(y8950_)
	, ram(config, name + " RAM", "Y8950 sample RAM", sampleRam)
	, clock(config.getMotherBoard().getCurrentTime())
	, volume(0)
{
	clearRam();
}

void Y8950Adpcm::clearRam()
{
	ram.clear(0xFF);
}

void Y8950Adpcm::reset(EmuTime::param time)
{
	removeSyncPoint();

	clock.reset(time);

	startAddr = 0;
	stopAddr = 7;
	delta = 0;
	addrMask = (1 << 18) - 1;
	reg7 = 0;
	reg15 = 0;
	readDelay = 0;
	romBank = false;
	writeReg(0x12, 255, time); // volume

	restart(emu);
	restart(aud);

	y8950.setStatus(Y8950::STATUS_BUF_RDY);
}

bool Y8950Adpcm::isPlaying() const
{
	return (reg7 & 0xC0) == 0x80;
}
bool Y8950Adpcm::isMuted() const
{
	return !isPlaying() || (reg7 & R07_SP_OFF);
}

void Y8950Adpcm::restart(PlayData& pd)
{
	pd.memPtr = startAddr;
	pd.nowStep = (1 << STEP_BITS) - delta;
	pd.out = 0;
	pd.output = 0;
	pd.diff = DIFF_DEFAULT;
	pd.nextLeveling = 0;
	pd.sampleStep = 0;
	pd.adpcm_data = 0; // dummy, avoid UMR in serialize
}

void Y8950Adpcm::sync(EmuTime::param time)
{
	if (isPlaying()) { // optimization, also correct without this test
		unsigned ticks = clock.getTicksTill(time);
		for (unsigned i = 0; isPlaying() && (i < ticks); ++i) {
			(void)calcSample(true); // ignore result
		}
	}
	clock.advance(time);
}

void Y8950Adpcm::schedule()
{
	assert(isPlaying());
	if ((stopAddr > startAddr) && (delta != 0)) {
		// TODO possible optimization, no need to set sync points if
		//      the corresponding bit is masked in the interrupt enable
		//      register
		if (reg7 & R07_MEMORY_DATA) {
			// we already did a sync(time), so clock is up-to-date
			Clock<Y8950::CLOCK_FREQ, Y8950::CLOCK_FREQ_DIV> stop(clock);
			uint64_t samples = stopAddr - emu.memPtr + 1;
			uint64_t length = (samples << STEP_BITS) +
					((1 << STEP_BITS) - emu.nowStep) +
					(delta - 1);
			stop += unsigned(length / delta);
			setSyncPoint(stop.getTime());
		} else {
			// TODO we should also set a sync-point in this case
			//      because this mode sets the STATUS_BUF_RDY bit
			//      which also triggers an IRQ
		}
	}
}

void Y8950Adpcm::executeUntil(EmuTime::param time)
{
	assert(isPlaying());
	sync(time); // should set STATUS_EOS
	assert(y8950.peekRawStatus() & Y8950::STATUS_EOS);
	if (isPlaying() && (reg7 & R07_REPEAT)) {
		schedule();
	}
}

void Y8950Adpcm::writeReg(byte rg, byte data, EmuTime::param time)
{
	sync(time); // TODO only when needed
	switch (rg) {
	case 0x07: // START/REC/MEM DATA/REPEAT/SP-OFF/-/-/RESET
		reg7 = data;
		if (reg7 & R07_START) {
			y8950.setStatus(Y8950::STATUS_PCM_BSY);
		} else {
			y8950.resetStatus(Y8950::STATUS_PCM_BSY);
		}
		if (reg7 & R07_RESET) {
			reg7 = 0;
		}
		if (reg7 & R07_START) {
			// start ADPCM
			restart(emu);
			restart(aud);
		}
		if (reg7 & R07_MEMORY_DATA) {
			// access external memory?
			emu.memPtr = startAddr;
			aud.memPtr = startAddr;
			readDelay = 2; // two dummy reads
			if ((reg7 & 0xA0) == 0x20) {
				// Memory read or write
				y8950.setStatus(Y8950::STATUS_BUF_RDY);
			}
		} else {
			// access via CPU
			emu.memPtr = 0;
			aud.memPtr = 0;
		}
		removeSyncPoint();
		if (isPlaying()) {
			schedule();
		}
		break;

	case 0x08: // CSM/KEY BOARD SPLIT/-/-/SAMPLE/DA AD/64K/ROM
		romBank = data & R08_ROM;
		addrMask = data & R08_64K ? (1 << 16) - 1 : (1 << 18) - 1;
		break;

	case 0x09: // START ADDRESS (L)
		startAddr = (startAddr & 0x7F807) | (data << 3);
		break;
	case 0x0A: // START ADDRESS (H)
		startAddr = (startAddr & 0x007FF) | (data << 11);
		break;

	case 0x0B: // STOP ADDRESS (L)
		stopAddr = (stopAddr & 0x7F807) | (data << 3);
		if (isPlaying()) {
			removeSyncPoint();
			schedule();
		}
		break;
	case 0x0C: // STOP ADDRESS (H)
		stopAddr = (stopAddr & 0x007FF) | (data << 11);
		if (isPlaying()) {
			removeSyncPoint();
			schedule();
		}
		break;

	case 0x0F: // ADPCM-DATA
		writeData(data);
		break;

	case 0x10: // DELTA-N (L)
		delta = (delta & 0xFF00) | data;
		volumeWStep = (volume * delta) >> STEP_BITS;
		if (isPlaying()) {
			removeSyncPoint();
			schedule();
		}
		break;
	case 0x11: // DELTA-N (H)
		delta = (delta & 0x00FF) | (data << 8);
		volumeWStep = (volume * delta) >> STEP_BITS;
		if (isPlaying()) {
			removeSyncPoint();
			schedule();
		}
		break;

	case 0x12: { // ENVELOP CONTROL
		volume = data;
		volumeWStep = (volume * delta) >> STEP_BITS;
		break;
	}
	case 0x0D: // PRESCALE (L)
	case 0x0E: // PRESCALE (H)
	case 0x15: // DAC-DATA  (bit9-2)
	case 0x16: //           (bit1-0)
	case 0x17: //           (exponent)
	case 0x1A: // PCM-DATA
		// not implemented
		break;
	}
}

void Y8950Adpcm::writeData(byte data)
{
	reg15 = data;
	if ((reg7 & R07_MODE) == 0x60) {
		// external memory write
		assert(!isPlaying()); // no need to update the 'aud' data
		if (readDelay) {
			emu.memPtr = startAddr;
			readDelay = 0;
		}
		if (emu.memPtr <= stopAddr) {
			writeMemory(emu.memPtr, data);
			emu.memPtr += 2; // two nibbles at a time

			// reset BRDY bit in status register,
			// which means we are processing the write
			y8950.resetStatus(Y8950::STATUS_BUF_RDY);

			// setup a timer that will callback us in 10
			// master clock cycles for Y8950. In the
			// callback set the BRDY flag to 1, which
			// means we have written the data. For now, we
			// don't really do this; we simply reset and
			// set the flag in zero time, so that the IRQ
			// will work.
			y8950.setStatus(Y8950::STATUS_BUF_RDY);

			if (emu.memPtr > stopAddr) {
				// we just received the last byte: set EOS
				// verified on real HW:
				//  in case of EOS, BUF_RDY is set as well
				y8950.setStatus(Y8950::STATUS_EOS);
				// Eugeny tested that pointer wraps when
				// continue writing after EOS
				emu.memPtr = startAddr;
			}
		}

	} else if ((reg7 & R07_MODE) == 0x80) {
		// ADPCM synthesis from CPU

		// Reset BRDY bit in status register, which means we
		// are full of data
		y8950.resetStatus(Y8950::STATUS_BUF_RDY);
	}
}

byte Y8950Adpcm::readReg(byte rg, EmuTime::param time)
{
	sync(time); // TODO only when needed
	byte result = (rg == 0x0F)
	            ? readData()   // ADPCM-DATA
	            : peekReg(rg); // other
	return result;
}

byte Y8950Adpcm::peekReg(byte rg, EmuTime::param time) const
{
	const_cast<Y8950Adpcm*>(this)->sync(time); // TODO only when needed
	return peekReg(rg);
}

byte Y8950Adpcm::peekReg(byte rg) const
{
	switch (rg) {
	case 0x0F: // ADPCM-DATA
		return peekData();
	case 0x13:
		// TODO check: is this before or after
		//   volume is applied
		//   filtering is performed
		return (emu.output >> 8) & 0xFF;
	case 0x14:
		return emu.output >> 16;
	default:
		return 255;
	}
}

void Y8950Adpcm::resetStatus()
{
	// If the BUF_RDY mask is cleared (e.g. by writing the value 0x80 to
	// register R#4). Reading the status register still has the BUF_RDY
	// bit set. Without this behavior demos like 'NOP Unknown reality'
	// hang when testing the amount of sample ram or when uploading data
	// to the sample ram.
	//
	// Before this code was added, those demos also worked but only
	// because we had a hack that always kept bit BUF_RDY set.
	//
	// When the ADPCM unit is not performing any function (e.g. after a
	// reset), the BUF_RDY bit should still be set. The AUDIO detection
	// routine in 'MSX-Audio BIOS v1.3' depends on this. See
	//   [3533002] Y8950 not being detected by MSX-Audio v1.3
	//   https://sourceforge.net/tracker/?func=detail&aid=3533002&group_id=38274&atid=421861
	// TODO I've implemented this as '(reg7 & R07_MODE) == 0', is this
	//      correct/complete?
	if (((reg7 & R07_MODE & ~R07_REC) == R07_MEMORY_DATA) ||
	    ((reg7 & R07_MODE) == 0)){
		// transfer to or from sample ram, or no function
		y8950.setStatus(Y8950::STATUS_BUF_RDY);
	}
}

byte Y8950Adpcm::readData()
{
	if ((reg7 & R07_MODE) == R07_MEMORY_DATA) {
		// external memory read
		assert(!isPlaying()); // no need to update the 'aud' data
		if (readDelay) {
			emu.memPtr = startAddr;
		}
	}
	byte result = peekData();
	if ((reg7 & R07_MODE) == R07_MEMORY_DATA) {
		assert(!isPlaying()); // no need to update the 'aud' data
		if (readDelay) {
			// two dummy reads
			--readDelay;
			y8950.setStatus(Y8950::STATUS_BUF_RDY);
		} else if (emu.memPtr > stopAddr) {
			// set EOS bit in status register
			y8950.setStatus(Y8950::STATUS_EOS);
		} else {
			emu.memPtr += 2; // two nibbles at a time

			// reset BRDY bit in status register, which means we
			// are reading the memory now
			y8950.resetStatus(Y8950::STATUS_BUF_RDY);

			// setup a timer that will callback us in 10 master
			// clock cycles for Y8950. In the callback set the BRDY
			// flag to 1, which means we have another data ready.
			// For now, we don't really do this; we simply reset and
			// set the flag in zero time, so that the IRQ will work.

			// set BRDY bit in status register
			y8950.setStatus(Y8950::STATUS_BUF_RDY);
		}
	}
	return result;
}

byte Y8950Adpcm::peekData() const
{
	if ((reg7 & R07_MODE) == R07_MEMORY_DATA) {
		// external memory read
		assert(!isPlaying()); // no need to update the 'aud' data
		if (readDelay) {
			return reg15;
		} else if (emu.memPtr > stopAddr) {
			return 0;
		} else {
			return readMemory(emu.memPtr);
		}
	} else {
		return 0; // TODO check
	}
}

void Y8950Adpcm::writeMemory(unsigned memPtr, byte value)
{
	unsigned addr = (memPtr / 2) & addrMask;
	if ((addr < ram.size()) && !romBank) {
		ram.write(addr, value);
	}
}
byte Y8950Adpcm::readMemory(unsigned memPtr) const
{
	unsigned addr = (memPtr / 2) & addrMask;
	if (romBank || (addr >= ram.size())) {
		return 0; // checked on a real machine
	} else {
		return ram[addr];
	}
}

int Y8950Adpcm::calcSample()
{
	// called by audio thread
	if (!isPlaying()) return 0;
	int output = calcSample(false);
	return (reg7 & R07_SP_OFF) ? 0 : output;
}

int Y8950Adpcm::calcSample(bool doEmu)
{
	// values taken from ymdelta.c by Tatsuyuki Satoh.
	static constexpr std::array<int, 16> F1 = {
		 1,   3,   5,   7,   9,  11,  13,  15,
		-1,  -3,  -5,  -7,  -9, -11, -13, -15
	};
	static constexpr std::array<int, 16> F2 = {
		57,  57,  57,  57,  77, 102, 128, 153,
		57,  57,  57,  57,  77, 102, 128, 153
	};

	assert(isPlaying());

	PlayData& pd = doEmu ? emu : aud;
	pd.nowStep += delta;
	if (pd.nowStep & ~STEP_MASK) {
		pd.nowStep &= STEP_MASK;
		byte val = [&] {
			if (!(pd.memPtr & 1)) {
				// even nibble
				if (reg7 & R07_MEMORY_DATA) {
					pd.adpcm_data = readMemory(pd.memPtr);
				} else {
					pd.adpcm_data = reg15;
					// set BRDY bit, ready to accept new data
					if (doEmu) {
						y8950.setStatus(Y8950::STATUS_BUF_RDY);
					}
				}
				return pd.adpcm_data >> 4;
			} else {
				// odd nibble
				return pd.adpcm_data & 0x0F;
			}
		}();
		int prevOut = pd.out;
		pd.out = Math::clipToInt16(pd.out + (pd.diff * F1[val]) / 8);
		pd.diff = std::clamp((pd.diff * F2[val]) / 64, DIFF_MIN, DIFF_MAX);

		int prevLeveling = pd.nextLeveling;
		pd.nextLeveling = (prevOut + pd.out) / 2;
		int deltaLeveling = pd.nextLeveling - prevLeveling;
		pd.sampleStep = deltaLeveling * volumeWStep;
		int tmp = deltaLeveling * ((volume * narrow<int>(pd.nowStep)) >> STEP_BITS);
		pd.output = prevLeveling * volume + tmp;

		++pd.memPtr;
		if ((reg7 & R07_MEMORY_DATA) &&
		    (pd.memPtr > stopAddr)) {
			// On 2003/06/21 I commited a patch with comment:
			//   generate end-of-sample interrupt at every sample
			//   end, including loops
			// Unfortunately it doesn't give any reason why and now
			// I can't remember it :-(
			// This is different from e.g. the MAME implementation.
			if (doEmu) {
				y8950.setStatus(Y8950::STATUS_EOS);
			}
			if (reg7 & R07_REPEAT) {
				restart(pd);
			} else {
				if (doEmu) {
					removeSyncPoint();
					reg7 = 0;
				}
			}
		}
	} else {
		pd.output += pd.sampleStep;
	}
	return pd.output >> 12;
}


// version 1:
//  Initial version
// version 2:
//  - Split PlayData in emu and audio part (though this doesn't add new state
//    to the savestate).
//  - Added clock object.
template<typename Archive>
void Y8950Adpcm::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("ram",          ram,
	             "startAddr",    startAddr,
	             "stopAddr",     stopAddr,
	             "addrMask",     addrMask,
	             "volume",       volume,
	             "volumeWStep",  volumeWStep,
	             "readDelay",    readDelay,
	             "delta",        delta,
	             "reg7",         reg7,
	             "reg15",        reg15,
	             "romBank",      romBank,

	             "memPntr",      emu.memPtr, // keep 'memPntr' for bw compat
	             "nowStep",      emu.nowStep,
	             "out",          emu.out,
	             "output",       emu.output,
	             "diff",         emu.diff,
	             "nextLeveling", emu.nextLeveling,
	             "sampleStep",   emu.sampleStep,
	             "adpcm_data",   emu.adpcm_data);
	if constexpr (Archive::IS_LOADER) {
		// ignore aud part for saving,
		// for loading we make it the same as the emu part
		aud = emu;
	}

	if (ar.versionBelow(version, 2)) {
		clock.reset(getCurrentTime());

		// reschedule, because automatically deserialized sync-point
		// can be off, because clock.getTime() != getCurrentTime()
		removeSyncPoint();
		if (isPlaying()) {
			schedule();
		}
	} else {
		ar.serialize("clock", clock);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Y8950Adpcm);

} // namespace openmsx
