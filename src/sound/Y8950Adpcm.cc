// $Id$

#include "Y8950Adpcm.hh"
#include "Y8950.hh"
#include "Scheduler.hh"


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

Y8950Adpcm::Y8950Adpcm(Y8950* y8950_, int sampleRam)
	: y8950(y8950_), volume(0)
{
	ramSize = sampleRam;
	ramBank = new byte[256*1024];
	romBank = new byte[256*1024];
	memset(ramBank, 255, 256*1024);
	memset(romBank, 255, 256*1024);
}

Y8950Adpcm::~Y8950Adpcm()
{
	delete[] ramBank;
	delete[] romBank;
}

void Y8950Adpcm::reset(const EmuTime &time)
{
	playing = false;
	startAddr = 0;
	stopAddr = 7;
	memPntr = 0;
	delta = 0;
	step = 0;
	wave = ramBank;
	addrMask = (1 << 19) - 1;
	reg7 = 0;
	reg15 = 0;
	writeReg(0x12, 255, time);	// volume
	restart();
}

void Y8950Adpcm::setSampleRate(int sr)
{
	sampleRate = sr;
}

bool Y8950Adpcm::muted()
{
	return (!playing) || (reg7 & R07_SP_OFF);
}

//**************************************************//
//                                                  //
//                       I/O Ctrl                   //
//                                                  //
//**************************************************//

void Y8950Adpcm::restart()
{
	playAddr = startAddr & addrMask;
	nowStep = MAX_STEP - step;
	out = output = 0;
	diff = DDEF;
	nextLeveling = 0;
	sampleStep = 0;
	volumeWStep = (int)((double)volume * step / MAX_STEP);
}

void Y8950Adpcm::schedule(const EmuTime &time)
{
	if (stopAddr > startAddr) {
		uint64 samples = stopAddr - playAddr + 1;
		EmuTimeFreq<Y8950::CLK_FREQ> stop(time);
		stop += (samples * (72 << 16) / delta);
		Scheduler::instance().setSyncPoint(stop, this);
	}
}

void Y8950Adpcm::executeUntil(const EmuTime& time, int userData) throw()
{
	y8950->setStatus(Y8950::STATUS_EOS);
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
			if (reg7 & R07_RESET) {
				playing = false;
			} else if (data & R07_START) {
				playing = true;
				restart();
			}
			
			if (playing) {
				schedule(time);
			} else {
				Scheduler::instance().removeSyncPoint(this);
			}
			break;

		case 0x08: // CSM/KEY BOARD SPLIT/-/-/SAMPLE/DA AD/64K/ROM 
			//wave = data&R08_ROM ? romBank : ramBank;
			addrMask = data&R08_64K ? (1<<17)-1 : (1<<19)-1;
			break;

		case 0x09: // START ADDRESS (L)
			startAddr = (startAddr & 0x7F800) | (data << 3);
			memPntr = 0;
			break;
		case 0x0A: // START ADDRESS (H) 
			startAddr = (startAddr & 0x007F8) | (data << 11);
			memPntr = 0;
			break;

		case 0x0B: // STOP ADDRESS (L)
			stopAddr = (stopAddr & 0x7F807) | (data << 3);
			break;
		case 0x0C: // STOP ADDRESS (H) 
			stopAddr = (stopAddr & 0x007FF) | (data << 11);
			break;


		case 0x0F: // ADPCM-DATA
			// TODO check this
			//if ((reg7 & R07_REC) && (reg7 & R07_MEMORY_DATA)) {
			{
				int tmp = ((startAddr + memPntr) & addrMask) / 2;
				tmp = (tmp < ramSize) ? tmp : (tmp & (ramSize - 1)); 
				wave[tmp] = data;
				//PRT_DEBUG("Y8950Adpcm: mem " << tmp << " " << (int)data);
				memPntr += 2;
				if ((startAddr + memPntr) > stopAddr) {
					y8950->setStatus(Y8950::STATUS_EOS);
				}
			}
			y8950->setStatus(Y8950::STATUS_BUF_RDY);
			break;

		case 0x10: // DELTA-N (L) 
			delta = (delta & 0xFF00) | data;
			step = Y8950::rate_adjust(delta<<GETA_BITS, sampleRate);
			volumeWStep = (int)((double)volume * step / MAX_STEP);
			break;
		case 0x11: // DELTA-N (H) 
			delta = (delta & 0x00FF) | (data << 8);
			step = Y8950::rate_adjust(delta<<GETA_BITS, sampleRate);
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

byte Y8950Adpcm::readReg(byte rg)
{
	byte result;
	switch (rg) {
		case 0x0F: { // ADPCM-DATA
			// TODO don't advance pointer when playing???
			int adr = ((startAddr + memPntr) & addrMask) / 2;
			result = wave[adr];
			memPntr += 2;
			if ((startAddr + memPntr) > stopAddr) {
				y8950->setStatus(Y8950::STATUS_EOS);
			}
			break;
		}
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
			nibble val;
			if (!(playAddr & 1)) {
				// n-th nibble
				reg15 = wave[playAddr / 2];
				val = reg15 >> 4;
			} else {
				// (n+1)-th nibble
				val = reg15 & 0x0F;
			}
			int prevOut = out;
			out = CLAP(DECODE_MIN, out + (diff * F1[val]) / 8,
			           DECODE_MAX);
			diff = CLAP(DMIN, (diff * F2[val]) / 64, DMAX);
			int deltaNext = out - prevOut;
			nowLeveling = nextLeveling;
			nextLeveling = prevOut + deltaNext / 2;
		
			playAddr++;
			if (playAddr > stopAddr) {
				if (reg7 & R07_REPEAT) {
					restart();
				} else {
					playing = false;
					//y8950->setStatus(Y8950::STATUS_EOS);
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

} // namespace openmsx

