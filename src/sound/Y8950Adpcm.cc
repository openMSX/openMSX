// $Id$

#include "Y8950Adpcm.hh"
#include "Y8950.hh"
#include "Debugger.hh"
#include "Clock.hh"

using std::string;

namespace openmsx {

// Relative volume between ADPCM part and FM part,
// value experimentally found by Manuel Bilderbeek
const int ADPCM_VOLUME = 356;

// Bitmask for register 0x07
static const int R07_RESET        = 0x01;
//static const int R07            = 0x02;.      // not used
//static const int R07            = 0x04;.      // not used
const int R07_SP_OFF       = 0x08;
const int R07_REPEAT       = 0x10;
const int R07_MEMORY_DATA  = 0x20;
const int R07_REC          = 0x40;
const int R07_START        = 0x80;
const int R07_MODE         = 0xE0;

//Bitmask for register 0x08
const int R08_ROM          = 0x01;
const int R08_64K          = 0x02;
const int R08_DA_AD        = 0x04;
const int R08_SAMPL        = 0x08;
//const int R08            = 0x10;.      // not used
//const int R08            = 0x20;.      // not used
const int R08_NOTE_SET     = 0x40;
const int R08_CSM          = 0x80;

const int DMAX = 0x6000;
const int DMIN = 0x7F;
const int DDEF = 0x7F;

const int DECODE_MAX = 32767;
const int DECODE_MIN = -32768;

const int GETA_BITS  = 14;
const unsigned int MAX_STEP   = 1<<(16+GETA_BITS);


//**************************************************//
//                                                  //
//  Helper functions                                //
//                                                  //
//**************************************************//

int Y8950Adpcm::CLAP(int min, int x, int max)
{
	return (x < min) ? min : ((max < x) ? max : x);
}

//**********************************************************//
//                                                          //
//  Y8950Adpcm                                              //
//                                                          //
//**********************************************************//

Y8950Adpcm::Y8950Adpcm(Y8950& y8950_, const string& name_, int sampleRam)
	: y8950(y8950_), name(name_ + " RAM"), ramSize(sampleRam), volume(0)
{
	ramBank = new byte[ramSize];
	memset(ramBank, 0xFF, ramSize);

	y8950.debugger.registerDebuggable(name, *this);
}

Y8950Adpcm::~Y8950Adpcm()
{
	removeSyncPoint();
	y8950.debugger.unregisterDebuggable(name, *this);
	delete[] ramBank;
}

void Y8950Adpcm::reset(const EmuTime &time)
{
	startAddr = 0;
	stopAddr = 7;
	delta = 0;
	step = 0;
	addrMask = (1 << 18) - 1;
	reg7 = 0;
	reg15 = 0;
	readDelay = 0;
	writeReg(0x12, 255, time);	// volume
	restart();
	y8950.setStatus(Y8950::STATUS_BUF_RDY);
}

void Y8950Adpcm::setSampleRate(int sr)
{
	sampleRate = sr;
}

bool Y8950Adpcm::playing() const
{
	return (reg7 & 0xC0) == 0x80;
}
bool Y8950Adpcm::muted() const
{
	return !playing() || (reg7 & R07_SP_OFF);
}

//**************************************************//
//                                                  //
//                       I/O Ctrl                   //
//                                                  //
//**************************************************//

void Y8950Adpcm::restart()
{
	memPntr = startAddr;
	nowStep = MAX_STEP - step;
	out = output = 0;
	diff = DDEF;
	nextLeveling = 0;
	sampleStep = 0;
	volumeWStep = (int)((double)volume * step / MAX_STEP);
}

void Y8950Adpcm::schedule(const EmuTime& time)
{
	if ((stopAddr > startAddr) && (delta != 0)) {
		uint64 samples = stopAddr - memPntr + 1;
		Clock<Y8950::CLK_FREQ> stop(time);
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

const string& Y8950Adpcm::schedName() const
{
	static const string name("Y8950Adpcm");
	return name;
}

void Y8950Adpcm::writeReg(byte rg, byte data, const EmuTime &time)
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
		step = Y8950::rate_adjust(delta << GETA_BITS, sampleRate);
		volumeWStep = (int)((double)volume * step / MAX_STEP);
		break;
	case 0x11: // DELTA-N (H)
		delta = (delta & 0x00FF) | (data << 8);
		step = Y8950::rate_adjust(delta << GETA_BITS, sampleRate);
		volumeWStep = (int)((double)volume * step / MAX_STEP);
		break;

	case 0x12: { // ENVELOP CONTROL
		int oldVol = volume;
		volume = (data * ADPCM_VOLUME) >> 8;
		if (oldVol != 0) {
			double factor = (double)volume / (double)oldVol;
			output =     (int)((double)output     * factor);
			sampleStep = (int)((double)sampleStep * factor);
		}
		volumeWStep = (int)((double)volume * step / MAX_STEP);
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
	byte result;
	switch (rg) {
		case 0x0F: // ADPCM-DATA
			result = readData();
			break;
		case 0x13: // TODO check
			result = out & 0xFF;
			break;
		case 0x14: // TODO check
			result = out / 256;
			break;
		default:
			result = 255;
	}
	//PRT_DEBUG("Y8950Adpcm: read "<<(int)rg<<" "<<(int)result);
	return result;
}

byte Y8950Adpcm::readData()
{
	byte result;
	if ((reg7 & R07_MODE) == R07_MEMORY_DATA) {
		// external memory read
		if (readDelay) {
			// two dummy reads
			memPntr = startAddr;
			--readDelay;
			result = reg15;
		} else if (memPntr > stopAddr) {
			// set EOS bit in status register
			y8950.setStatus(Y8950::STATUS_EOS);
			result = 0;
		} else {
			result = readMemory();
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
	} else {
		result = 0; // TODO check
	}
	return result;
}

void Y8950Adpcm::writeMemory(byte value)
{
	int addr = (memPntr / 2) & addrMask;
	if ((addr < ramSize) && !romBank) {
		ramBank[addr] = value;
	}
}
byte Y8950Adpcm::readMemory()
{
	int addr = (memPntr / 2) & addrMask;
	if (romBank || (addr >= ramSize)) {
		return 0; // checked on a real machine
	} else {
		return ramBank[addr];
	}
}

int Y8950Adpcm::calcSample()
{
	// This table values are from ymdelta.c by Tatsuyuki Satoh.
	static const int F1[16] = { 1,   3,   5,   7,   9,  11,  13,  15,
				   -1,  -3,  -5,  -7,  -9, -11, -13, -15};
	static const int F2[16] = {57,  57,  57,  57,  77, 102, 128, 153,
				   57,  57,  57,  57,  77, 102, 128, 153};

	if (muted()) {
		return 0;
	}
	nowStep += step;
	if (nowStep >= MAX_STEP) {
		int nowLeveling;
		do {
			nowStep -= MAX_STEP;
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
			out = CLAP(DECODE_MIN, out + (diff * F1[val]) / 8,
			           DECODE_MAX);
			diff = CLAP(DMIN, (diff * F2[val]) / 64, DMAX);
			int deltaNext = out - prevOut;
			nowLeveling = nextLeveling;
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
		} while (nowStep >= MAX_STEP);
		sampleStep = (nextLeveling - nowLeveling) * volumeWStep;
		output = nowLeveling * volume;
		output += (int)((double)sampleStep * ((double)nowStep/(double)step));
	}
	output += sampleStep;
	return output >> 12;
}


// Debuggable
unsigned Y8950Adpcm::getSize() const
{
	return ramSize;
}

const string& Y8950Adpcm::getDescription() const
{
	static const string desc = "Y8950 sample RAM";
	return desc;
}

byte Y8950Adpcm::read(unsigned address)
{
	return ramBank[address];
}

void Y8950Adpcm::write(unsigned address, byte value)
{
	ramBank[address] = value;
}

} // namespace openmsx

