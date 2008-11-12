// $Id$

#include "Y8950Adpcm.hh"
#include "Y8950.hh"
#include "Clock.hh"
#include "Ram.hh"
#include "MSXMotherBoard.hh"
#include "Math.hh"
#include "serialize.hh"
#include <cstring>

namespace openmsx {

// Relative volume between ADPCM part and FM part,
// value experimentally found by Manuel Bilderbeek
static const int ADPCM_VOLUME = 356;

// Bitmask for register 0x07
static const int R07_RESET        = 0x01;
static const int R07_SP_OFF       = 0x08;
static const int R07_REPEAT       = 0x10;
static const int R07_MEMORY_DATA  = 0x20;
static const int R07_REC          = 0x40;
static const int R07_START        = 0x80;
static const int R07_MODE         = 0xE0;

// Bitmask for register 0x08
static const int R08_ROM          = 0x01;
static const int R08_64K          = 0x02;
static const int R08_DA_AD        = 0x04;
static const int R08_SAMPL        = 0x08;
static const int R08_NOTE_SET     = 0x40;
static const int R08_CSM          = 0x80;

static const int DMAX = 0x6000;
static const int DMIN = 0x7F;
static const int DDEF = 0x7F;

static const int STEP_BITS = 16;
static const int STEP_MASK = (1 << STEP_BITS) -1;


Y8950Adpcm::Y8950Adpcm(Y8950& y8950_, MSXMotherBoard& motherBoard,
                       const std::string& name, unsigned sampleRam)
	: Schedulable(motherBoard.getScheduler())
	, y8950(y8950_)
	, ram(new Ram(motherBoard, name + " RAM", "Y8950 sample RAM", sampleRam))
	, volume(0)
{
	memset(&(*ram)[0], 0xFF, ram->getSize());

	// avoid (harmless) UMR in serialize()
	adpcm_data = 0;
}

Y8950Adpcm::~Y8950Adpcm()
{
}

void Y8950Adpcm::reset(const EmuTime& time)
{
	startAddr = 0;
	stopAddr = 7;
	delta = 0;
	addrMask = (1 << 18) - 1;
	reg7 = 0;
	reg15 = 0;
	readDelay = 0;
	romBank = false;
	writeReg(0x12, 255, time); // volume
	restart();
	y8950.setStatus(Y8950::STATUS_BUF_RDY);
}

bool Y8950Adpcm::playing() const
{
	return (reg7 & 0xC0) == 0x80;
}
bool Y8950Adpcm::muted() const
{
	return !playing() || (reg7 & R07_SP_OFF);
}

void Y8950Adpcm::restart()
{
	memPntr = startAddr;
	nowStep = (1 << STEP_BITS) - delta;
	out = output = 0;
	diff = DDEF;
	nextLeveling = 0;
	sampleStep = 0;
	volumeWStep = (volume * delta) >> STEP_BITS;
}

void Y8950Adpcm::schedule(const EmuTime& time)
{
	if ((stopAddr > startAddr) && (delta != 0)) {
		uint64 samples = stopAddr - memPntr + 1;
		Clock<Y8950::CLOCK_FREQ> stop(time);
		stop += (samples * (72 << 16) / delta);
		setSyncPoint(stop.getTime());
	}
}

void Y8950Adpcm::executeUntil(const EmuTime& time, int /*userData*/)
{
	y8950.setStatus(Y8950::STATUS_EOS);
	if (reg7 & R07_REPEAT) {
		schedule(time);
	}
}

const std::string& Y8950Adpcm::schedName() const
{
	static const std::string name("Y8950Adpcm");
	return name;
}

void Y8950Adpcm::writeReg(byte rg, byte data, const EmuTime& time)
{
	//PRT_DEBUG("Y8950Adpcm: write "<<(int)rg<<" "<<(int)data);
	switch (rg) {
	case 0x07: // START/REC/MEM DATA/REPEAT/SP-OFF/-/-/RESET
		reg7 = data;
		if (reg7 & R07_START) {
			// start ADPCM
			restart();
		}
		if (reg7 & R07_MEMORY_DATA) {
			// access external memory?
			memPntr = startAddr;
			readDelay = 2; // two dummy reads
		} else {
			// access via CPU
			memPntr = 0;
		}
		if (reg7 & R07_RESET) {
			reg7 = 0;
			y8950.setStatus(Y8950::STATUS_BUF_RDY);
		}
		if (playing()) {
			schedule(time);
		} else {
			removeSyncPoint();
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
		break;
	case 0x0C: // STOP ADDRESS (H)
		stopAddr = (stopAddr & 0x007FF) | (data << 11);
		break;

	case 0x0F: // ADPCM-DATA
		writeData(data);
		break;

	case 0x10: // DELTA-N (L)
		delta = (delta & 0xFF00) | data;
		volumeWStep = (volume * delta) >> STEP_BITS;
		break;
	case 0x11: // DELTA-N (H)
		delta = (delta & 0x00FF) | (data << 8);
		volumeWStep = (volume * delta) >> STEP_BITS;
		break;

	case 0x12: { // ENVELOP CONTROL
		int oldVol = volume;
		volume = (data * ADPCM_VOLUME) >> 8;
		if (oldVol != 0) {
			double factor = double(volume) / double(oldVol);
			output     = int(double(output)     * factor);
			sampleStep = int(double(sampleStep) * factor);
		}
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
	if ((reg7 & 0xE0) == 0x60) {
		// external memory write
		if (readDelay) {
			memPntr = startAddr;
			readDelay = 0;
		}
		if (memPntr <= stopAddr) {
			writeMemory(data);
			memPntr += 2; // two nibbles at a time

			// reset BRDY bit in status register,
			// which means we are processing the write
			y8950.resetStatus(Y8950::STATUS_BUF_RDY);

			// setup a timer that will callback us in 10
			// master clock cycles for Y8950. In the
			// callback set the BRDY flag to 1 , which
			// means we have written the data. For now, we
			// don't really do this; we simply reset and
			// set the flag in zero time, so that the IRQ
			// will work.

			// set BRDY bit in status register
			y8950.setStatus(Y8950::STATUS_BUF_RDY);
		} else {
			// set EOS bit in status register
			y8950.setStatus(Y8950::STATUS_EOS);
		}

	} else if ((reg7 & 0xE0) == 0x80) {
		// ADPCM synthesis from CPU

		// Reset BRDY bit in status register, which means we
		// are full of data
		y8950.resetStatus(Y8950::STATUS_BUF_RDY);
	}
}

byte Y8950Adpcm::readReg(byte rg)
{
	byte result = (rg == 0x0F)
	            ? readData()   // ADPCM-DATA
	            : peekReg(rg); // other
	//PRT_DEBUG("Y8950Adpcm: read "<<(int)rg<<" "<<(int)result);
	return result;
}

byte Y8950Adpcm::peekReg(byte rg) const
{
	switch (rg) {
	case 0x0F: // ADPCM-DATA
		return peekData();
	case 0x13: // TODO check
		return out & 0xFF;
	case 0x14: // TODO check
		return out >> 8;
	default:
		return 255;
	}
}

byte Y8950Adpcm::readData()
{
	if ((reg7 & R07_MODE) == R07_MEMORY_DATA) {
		// external memory read
		if (readDelay) {
			memPntr = startAddr;
		}
	}
	byte result = peekData();
	if ((reg7 & R07_MODE) == R07_MEMORY_DATA) {
		if (readDelay) {
			// two dummy reads
			--readDelay;
		} else if (memPntr > stopAddr) {
			// set EOS bit in status register
			y8950.setStatus(Y8950::STATUS_EOS);
		} else {
			memPntr += 2; // two nibbles at a time

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
		if (readDelay) {
			return reg15;
		} else if (memPntr > stopAddr) {
			return 0;
		} else {
			return readMemory();
		}
	} else {
		return 0; // TODO check
	}
}

void Y8950Adpcm::writeMemory(byte value)
{
	unsigned addr = (memPntr / 2) & addrMask;
	if ((addr < ram->getSize()) && !romBank) {
		(*ram)[addr] = value;
	}
}
byte Y8950Adpcm::readMemory() const
{
	unsigned addr = (memPntr / 2) & addrMask;
	if (romBank || (addr >= ram->getSize())) {
		return 0; // checked on a real machine
	} else {
		return (*ram)[addr];
	}
}

int Y8950Adpcm::calcSample()
{
	// This table values are from ymdelta.c by Tatsuyuki Satoh.
	static const int F1[16] = {  1,   3,   5,   7,   9,  11,  13,  15,
	                            -1,  -3,  -5,  -7,  -9, -11, -13, -15 };
	static const int F2[16] = { 57,  57,  57,  57,  77, 102, 128, 153,
	                            57,  57,  57,  57,  77, 102, 128, 153 };

	if (muted()) {
		return 0;
	}
	nowStep += delta;
	if (nowStep & ~STEP_MASK) {
		nowStep &= STEP_MASK;
		byte val;
		if (!(memPntr & 1)) {
			// n-th nibble
			if (reg7 & R07_MEMORY_DATA) {
				adpcm_data = readMemory();
			} else {
				adpcm_data = reg15;
				// set BRDY bit, ready to accept new data
				y8950.setStatus(Y8950::STATUS_BUF_RDY);
			}
			val = adpcm_data >> 4;
		} else {
			// (n+1)-th nibble
			val = adpcm_data & 0x0F;
		}
		int prevOut = out;
		out = Math::clipIntToShort(out + (diff * F1[val]) / 8);
		diff = Math::clip<DMIN, DMAX>((diff * F2[val]) / 64);
		int deltaNext = out - prevOut;
		int nowLeveling = nextLeveling;
		nextLeveling = prevOut + deltaNext / 2;

		memPntr++;
		if ((reg7 & R07_MEMORY_DATA) &&
		    (memPntr > stopAddr)) {
			if (reg7 & R07_REPEAT) {
				restart();
			} else {
				reg7 = 0;
				// decoupled from audio thread
				//y8950.setStatus(Y8950::STATUS_EOS);
			}
		}
		int deltaLeveling = nextLeveling - nowLeveling;
		sampleStep = deltaLeveling * volumeWStep;
		int tmp = deltaLeveling * ((volume * nowStep) >> STEP_BITS);
		output = nowLeveling * volume + tmp;
	}
	output += sampleStep;
	return output >> 12;
}


template<typename Archive>
void Y8950Adpcm::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("ram", *ram);
	ar.serialize("startAddr", startAddr);
	ar.serialize("stopAddr", stopAddr);
	ar.serialize("addrMask", addrMask);
	ar.serialize("memPntr", memPntr);
	ar.serialize("nowStep", nowStep);
	ar.serialize("volume", volume);
	ar.serialize("out", out);
	ar.serialize("output", output);
	ar.serialize("diff", diff);
	ar.serialize("nextLeveling", nextLeveling);
	ar.serialize("sampleStep", sampleStep);
	ar.serialize("volumeWStep", volumeWStep);
	ar.serialize("readDelay", readDelay);
	ar.serialize("delta", delta);
	ar.serialize("reg7", reg7);
	ar.serialize("reg15", reg15);
	ar.serialize("adpcm_data", adpcm_data);
	ar.serialize("romBank", romBank);
}
INSTANTIATE_SERIALIZE_METHODS(Y8950Adpcm);

} // namespace openmsx
