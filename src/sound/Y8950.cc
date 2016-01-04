/*
  * Based on:
  *    emu8950.c -- Y8950 emulator written by Mitsutaka Okazaki 2001
  * heavily rewritten to fit openMSX structure
  */

#include "Y8950.hh"
#include "Y8950Periphery.hh"
#include "MSXAudio.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "Math.hh"
#include "outer.hh"
#include "serialize.hh"
#include <algorithm>
#include <cmath>

namespace openmsx {

static const unsigned EG_MUTE = 1 << Y8950::EG_BITS;
static const Y8950::EnvPhaseIndex EG_DP_MAX = Y8950::EnvPhaseIndex(EG_MUTE);

static const unsigned MOD = 0;
static const unsigned CAR = 1;

static const float EG_STEP = 0.1875f; //  3/16
static const float SL_STEP = 3.0f;
static const float TL_STEP = 0.75f;   // 12/16
static const float DB_STEP = 0.1875f; //  3/16

static const unsigned SL_PER_EG = 16; // SL_STEP / EG_STEP
static const unsigned TL_PER_EG =  4; // TL_STEP / EG_STEP
static const unsigned EG_PER_DB =  1; // EG_STEP / DB_STEP

// PM speed(Hz) and depth(cent)
static const float PM_SPEED  = 6.4f;
static const float PM_DEPTH  = 13.75f / 2;
static const float PM_DEPTH2 = 13.75f;

// Dynamic range of sustine level
static const int SL_BITS = 4;
static const int SL_MUTE = 1 << SL_BITS;
// Size of Sintable ( 1 -- 18 can be used, but 7 -- 14 recommended.)
static const int PG_BITS = 10;
static const int PG_WIDTH = 1 << PG_BITS;
static const int PG_MASK = PG_WIDTH - 1;
// Phase increment counter
static const int DP_BITS = 19;
static const int DP_BASE_BITS = DP_BITS - PG_BITS;

// WaveTable for each envelope amp.
//  values are in range[        0,   DB_MUTE)   (for positive values)
//                  or [2*DB_MUTE, 3*DB_MUTE)   (for negative values)
static unsigned sintable[PG_WIDTH];

// Phase incr table for Attack.
static Y8950::EnvPhaseIndex dphaseARTable[16][16];
// Phase incr table for Decay and Release.
static Y8950::EnvPhaseIndex dphaseDRTable[16][16];

// TL Table.
static int tllTable[16 * 8][4];

// Linear to Log curve conversion table (for Attack rate) and vice versa.
//   values are in the range [0 .. EG_MUTE]
static unsigned AR_ADJUST_TABLE[EG_MUTE];
static unsigned RA_ADJUST_TABLE[EG_MUTE + 1];

// Dynamic range
static const int DB_BITS = 9;
static const int DB_MUTE = 1 << DB_BITS;
// PM table is calcurated by PM_AMP * exp2(PM_DEPTH * sin(x) / 1200)
static const int PM_AMP_BITS = 8;
static const int PM_AMP = 1 << PM_AMP_BITS;

// Bits for liner value
static const int DB2LIN_AMP_BITS = 11;
static const int SLOT_AMP_BITS = DB2LIN_AMP_BITS;

// Bits for Pitch and Amp modulator
static const int PM_PG_BITS = 8;
static const int PM_PG_WIDTH = 1 << PM_PG_BITS;
static const int PM_DP_BITS = 16;
static const int PM_DP_WIDTH = 1 << PM_DP_BITS;
static const int AM_PG_BITS = 8;
static const int AM_PG_WIDTH = 1 << AM_PG_BITS;
static const int AM_DP_BITS = 16;
static const int AM_DP_WIDTH = 1 << AM_DP_BITS;

// LFO Table
static const unsigned PM_DPHASE = unsigned(PM_SPEED * PM_DP_WIDTH / (Y8950::CLOCK_FREQ / float(Y8950::CLOCK_FREQ_DIV)));
static int pmtable[2][PM_PG_WIDTH];

// dB to Liner table
static int dB2LinTab[(2 * DB_MUTE) * 2];


// LFO Amplitude Modulation table (verified on real YM3812)
// 27 output levels (triangle waveform);
// 1 level takes one of: 192, 256 or 448 samples
//
// Length: 210 elements.
//  Each of the elements has to be repeated
//  exactly 64 times (on 64 consecutive samples).
//  The whole table takes: 64 * 210 = 13440 samples.
//
// Verified on real YM3812 (OPL2), but I believe it's the same for Y8950
// because it closely matches the Y8950 AM parameters:
//    speed = 3.7Hz
//    depth = 4.875dB
// Also this approch can be easily implemented in HW, the previous one (see SVN
// history) could not.
static const unsigned LFO_AM_TAB_ELEMENTS = 210;
static const byte lfo_am_table[LFO_AM_TAB_ELEMENTS] =
{
	0,0,0,0,0,0,0,
	1,1,1,1,
	2,2,2,2,
	3,3,3,3,
	4,4,4,4,
	5,5,5,5,
	6,6,6,6,
	7,7,7,7,
	8,8,8,8,
	9,9,9,9,
	10,10,10,10,
	11,11,11,11,
	12,12,12,12,
	13,13,13,13,
	14,14,14,14,
	15,15,15,15,
	16,16,16,16,
	17,17,17,17,
	18,18,18,18,
	19,19,19,19,
	20,20,20,20,
	21,21,21,21,
	22,22,22,22,
	23,23,23,23,
	24,24,24,24,
	25,25,25,25,
	26,26,26,
	25,25,25,25,
	24,24,24,24,
	23,23,23,23,
	22,22,22,22,
	21,21,21,21,
	20,20,20,20,
	19,19,19,19,
	18,18,18,18,
	17,17,17,17,
	16,16,16,16,
	15,15,15,15,
	14,14,14,14,
	13,13,13,13,
	12,12,12,12,
	11,11,11,11,
	10,10,10,10,
	9,9,9,9,
	8,8,8,8,
	7,7,7,7,
	6,6,6,6,
	5,5,5,5,
	4,4,4,4,
	3,3,3,3,
	2,2,2,2,
	1,1,1,1
};

//**************************************************//
//                                                  //
//  Helper functions                                //
//                                                  //
//**************************************************//

static inline unsigned DB_POS(int x)
{
	int result = int(x / DB_STEP);
	assert(result < DB_MUTE);
	assert(result >= 0);
	return result;
}
static inline unsigned DB_NEG(int x)
{
	return 2 * DB_MUTE + DB_POS(x);
}

//**************************************************//
//                                                  //
//                  Create tables                   //
//                                                  //
//**************************************************//

// Return log_BASE(x)  IOW the value y so that pow(BASE, y) == x.
template<int BASE> static inline float log(float x)
{
	return ::logf(x) / ::logf(float(BASE));
}

// Table for AR to LogCurve and vice versa.
static void makeAdjustTable()
{
	AR_ADJUST_TABLE[0] = EG_MUTE;
	RA_ADJUST_TABLE[0] = EG_MUTE;
	for (int i = 1; i < int(EG_MUTE); ++i) {
		AR_ADJUST_TABLE[i] = (EG_MUTE - 1 - EG_MUTE * log<EG_MUTE>(i)) / 2;
		RA_ADJUST_TABLE[i] = powf(EG_MUTE, float(EG_MUTE - 1 - 2 * i) / EG_MUTE);
		assert(0 <= int(AR_ADJUST_TABLE[i]));
		assert(0 <= int(RA_ADJUST_TABLE[i]));
		assert(AR_ADJUST_TABLE[i] <= EG_MUTE);
		assert(RA_ADJUST_TABLE[i] <= EG_MUTE);
	}
	RA_ADJUST_TABLE[EG_MUTE] = 0;

	// AR_ADJUST_TABLE[] and RA_ADJUST_TABLE[] are each others inverse, IOW
	//   RA_ADJUST_TABLE[AR_ADJUST_TABLE[x]] == x
	// (except for rounding errors).
}

// Table for dB(0 -- (1<<DB_BITS)) to Liner(0 -- DB2LIN_AMP_WIDTH)
static void makeDB2LinTable()
{
	for (int i = 0; i < DB_MUTE; ++i) {
		dB2LinTab[i] = int(float((1 << DB2LIN_AMP_BITS) - 1) *
		                   powf(10, -float(i) * DB_STEP / 20));
	}
	assert(dB2LinTab[DB_MUTE - 1] == 0);
	for (int i = DB_MUTE; i < 2 * DB_MUTE; ++i) {
		dB2LinTab[i] = 0;
	}
	for (int i = 0; i < 2 * DB_MUTE; ++i) {
		dB2LinTab[i + 2 * DB_MUTE] = -dB2LinTab[i];
	}
}

// Liner(+0.0 - +1.0) to dB(DB_MUTE-1 -- 0)
static unsigned lin2db(float d)
{
	if (d < 1e-4f) {
		// (almost) zero
		return DB_MUTE - 1;
	}
	int tmp = -int(20.0f * log10f(d) / DB_STEP);
	int result = std::min(tmp, DB_MUTE - 1);
	assert(result >= 0);
	assert(result <= DB_MUTE - 1);
	return result;
}

// Sin Table
static void makeSinTable()
{
	for (int i = 0; i < PG_WIDTH / 4; ++i) {
		sintable[i] = lin2db(sinf(float(2.0 * M_PI) * i / PG_WIDTH));
	}
	for (int i = 0; i < PG_WIDTH / 4; i++) {
		sintable[PG_WIDTH / 2 - 1 - i] = sintable[i];
	}
	for (int i = 0; i < PG_WIDTH / 2; i++) {
		sintable[PG_WIDTH / 2 + i] = 2 * DB_MUTE + sintable[i];
	}
}

// Table for Pitch Modulator
static void makePmTable()
{
	for (int i = 0; i < PM_PG_WIDTH; ++i) {
		pmtable[0][i] = int(float(PM_AMP) * exp2f(float(PM_DEPTH)  * sinf(float(2.0 * M_PI) * i / PM_PG_WIDTH) / 1200));
		pmtable[1][i] = int(float(PM_AMP) * exp2f(float(PM_DEPTH2) * sinf(float(2.0 * M_PI) * i / PM_PG_WIDTH) / 1200));
	}
}

static void makeTllTable()
{
	// Processed version of Table 3.5 from the Application Manual
	static const unsigned kltable[16] = {
		0, 24, 32, 37, 40, 43, 45, 47, 48, 50, 51, 52, 53, 54, 55, 56
	};
	// This is indeed {0.0, 3.0, 1.5, 6.0} dB/oct, verified on real Y8950.
	// Note the illogical order of 2nd and 3rd element.
	static const unsigned shift[4] = { 31, 1, 2, 0 };

	for (unsigned freq = 0; freq < 16 * 8; ++freq) {
		unsigned fnum  = freq % 16;
		unsigned block = freq / 16;
		int tmp = 4 * kltable[fnum] - 32 * (7 - block);
		for (unsigned KL = 0; KL < 4; ++KL) {
			tllTable[freq][KL] = (tmp <= 0) ? 0 : (tmp >> shift[KL]);
		}
	}
}

// Rate Table for Attack
static void makeDphaseARTable()
{
	for (unsigned Rks = 0; Rks < 16; ++Rks) {
		dphaseARTable[Rks][0] = Y8950::EnvPhaseIndex(0);
		for (unsigned AR = 1; AR < 15; ++AR) {
			unsigned RM = std::min(AR + (Rks >> 2), 15u);
			unsigned RL = Rks & 3;
			dphaseARTable[Rks][AR] =
				Y8950::EnvPhaseIndex(12 * (RL + 4)) >> (15 - RM);
		}
		dphaseARTable[Rks][15] = EG_DP_MAX;
	}
}

// Rate Table for Decay
static void makeDphaseDRTable()
{
	for (unsigned Rks = 0; Rks < 16; ++Rks) {
		dphaseDRTable[Rks][0] = Y8950::EnvPhaseIndex(0);
		for (unsigned DR = 1; DR < 16; ++DR) {
			unsigned RM = std::min(DR + (Rks >> 2), 15u);
			unsigned RL = Rks & 3;
			dphaseDRTable[Rks][DR] =
				Y8950::EnvPhaseIndex(RL + 4) >> (15 - RM);
		}
	}
}


// class Y8950::Patch

Y8950::Patch::Patch()
{
	reset();
}

void Y8950::Patch::reset()
{
	AM = false;
	PM = false;
	EG = false;
	ML = 0;
	KL = 0;
	TL = 0;
	AR = 0;
	DR = 0;
	SL = 0;
	RR = 0;
	setKeyScaleRate(false);
	setFeedbackShift(0);
}


// class Y8950::Slot

void Y8950::Slot::reset()
{
	phase = 0;
	output = 0;
	feedback = 0;
	eg_mode = FINISH;
	eg_phase = EG_DP_MAX;
	key = 0;
	patch.reset();

	// this initializes:
	//   dphase, tll, dphaseARTableRks, dphaseDRTableRks, eg_dphase
	updateAll(0);
}

void Y8950::Slot::updatePG(unsigned freq)
{
	static const int mltable[16] = {
		  1, 1*2,  2*2,  3*2,  4*2,  5*2,  6*2 , 7*2,
		8*2, 9*2, 10*2, 10*2, 12*2, 12*2, 15*2, 15*2
	};

	unsigned fnum  = freq % 1024;
	unsigned block = freq / 1024;
	dphase = ((fnum * mltable[patch.ML]) << block) >> (21 - DP_BITS);
}

void Y8950::Slot::updateTLL(unsigned freq)
{
	tll = tllTable[freq >> 6][patch.KL] + patch.TL * TL_PER_EG;
}

void Y8950::Slot::updateRKS(unsigned freq)
{
	unsigned rks = freq >> patch.KR;
	assert(rks < 16);
	dphaseARTableRks = dphaseARTable[rks];
	dphaseDRTableRks = dphaseDRTable[rks];
}

void Y8950::Slot::updateEG()
{
	switch (eg_mode) {
	case ATTACK:
		eg_dphase = dphaseARTableRks[patch.AR];
		break;
	case DECAY:
		eg_dphase = dphaseDRTableRks[patch.DR];
		break;
	case SUSTAIN:
	case RELEASE:
		eg_dphase = dphaseDRTableRks[patch.RR];
		break;
	case FINISH:
		eg_dphase = Y8950::EnvPhaseIndex(0);
		break;
	}
}

void Y8950::Slot::updateAll(unsigned freq)
{
	updatePG(freq);
	updateTLL(freq);
	updateRKS(freq);
	updateEG(); // EG should be last
}

bool Y8950::Slot::isActive() const
{
	return eg_mode != FINISH;
}

// Slot key on
void Y8950::Slot::slotOn(KeyPart part)
{
	if (!key) {
		eg_mode = ATTACK;
		phase = 0;
		eg_phase = Y8950::EnvPhaseIndex(RA_ADJUST_TABLE[eg_phase.toInt()]);
	}
	key |= part;
}

// Slot key off
void Y8950::Slot::slotOff(KeyPart part)
{
	if (key) {
		key &= ~part;
		if (!key) {
			if (eg_mode == ATTACK) {
				eg_phase = Y8950::EnvPhaseIndex(AR_ADJUST_TABLE[eg_phase.toInt()]);
			}
			eg_mode = RELEASE;
		}
	}
}


// class Y8950::Channel

Y8950::Channel::Channel()
{
	reset();
}

void Y8950::Channel::reset()
{
	setFreq(0);
	slot[MOD].reset();
	slot[CAR].reset();
	alg = false;
}

// Set frequency (combined F-Number (10bit) and Block (3bit))
void Y8950::Channel::setFreq(unsigned freq_)
{
	freq = freq_;
}

void Y8950::Channel::keyOn(KeyPart part)
{
	slot[MOD].slotOn(part);
	slot[CAR].slotOn(part);
}

void Y8950::Channel::keyOff(KeyPart part)
{
	slot[MOD].slotOff(part);
	slot[CAR].slotOff(part);
}


Y8950::Y8950(const std::string& name_, const DeviceConfig& config,
             unsigned sampleRam, EmuTime::param time, MSXAudio& audio)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "MSX-AUDIO", 9 + 5 + 1)
	, motherBoard(config.getMotherBoard())
	, periphery(audio.createPeriphery(getName()))
	, adpcm(*this, config, name_, sampleRam)
	, connector(motherBoard.getPluggingController())
	, dac13(name_ + " DAC", "MSX-AUDIO 13-bit DAC", config)
	, debuggable(motherBoard, getName())
	, timer1(EmuTimer::createOPL3_1(motherBoard.getScheduler(), *this))
	, timer2(EmuTimer::createOPL3_2(motherBoard.getScheduler(), *this))
	, irq(motherBoard, getName() + ".IRQ")
	, enabled(true)
{
	makePmTable();
	makeAdjustTable();
	makeDB2LinTable();
	makeTllTable();
	makeSinTable();

	makeDphaseARTable();
	makeDphaseDRTable();

	float input = Y8950::CLOCK_FREQ / float(Y8950::CLOCK_FREQ_DIV);
	setInputRate(int(input + 0.5f));

	reset(time);
	registerSound(config);
}

Y8950::~Y8950()
{
	unregisterSound();
}

void Y8950::clearRam()
{
	adpcm.clearRam();
}

// Reset whole of opl except patch datas.
void Y8950::reset(EmuTime::param time)
{
	for (auto& c : ch) c.reset();

	rythm_mode = false;
	am_mode = false;
	pm_mode = false;
	pm_phase = 0;
	am_phase = 0;
	noise_seed = 0xffff;
	noiseA_phase = 0;
	noiseB_phase = 0;
	noiseA_dphase = 0;
	noiseB_dphase = 0;

	// update the output buffer before changing the register
	updateStream(time);
	for (auto& r : reg) r = 0x00;

	reg[0x04] = 0x18;
	reg[0x19] = 0x0F; // fixes 'Thunderbirds are Go'
	status = 0x00;
	statusMask = 0;
	irq.reset();

	adpcm.reset(time);
}


// Drum key on
void Y8950::keyOn_BD()  { ch[6].keyOn(KEY_RHYTHM); }
void Y8950::keyOn_HH()  { ch[7].slot[MOD].slotOn(KEY_RHYTHM); }
void Y8950::keyOn_SD()  { ch[7].slot[CAR].slotOn(KEY_RHYTHM); }
void Y8950::keyOn_TOM() { ch[8].slot[MOD].slotOn(KEY_RHYTHM); }
void Y8950::keyOn_CYM() { ch[8].slot[CAR].slotOn(KEY_RHYTHM); }

// Drum key off
void Y8950::keyOff_BD() { ch[6].keyOff(KEY_RHYTHM); }
void Y8950::keyOff_HH() { ch[7].slot[MOD].slotOff(KEY_RHYTHM); }
void Y8950::keyOff_SD() { ch[7].slot[CAR].slotOff(KEY_RHYTHM); }
void Y8950::keyOff_TOM(){ ch[8].slot[MOD].slotOff(KEY_RHYTHM); }
void Y8950::keyOff_CYM(){ ch[8].slot[CAR].slotOff(KEY_RHYTHM); }

// Change Rhythm Mode
void Y8950::setRythmMode(int data)
{
	bool newMode = (data & 32) != 0;
	if (rythm_mode != newMode) {
		rythm_mode = newMode;
		if (!rythm_mode) {
			// ON->OFF
			keyOff_BD();  // TODO keyOff() or immediately to FINISH?
			keyOff_HH();  //      other variants use keyOff(), but
			keyOff_SD();  //      verify on real HW
			keyOff_TOM();
			keyOff_CYM();
		}
	}
}

// recalculate 'key' from register settings
void Y8950::update_key_status()
{
	for (unsigned i = 0; i < 9; ++i) {
		int main = (reg[0xb0 + i] & 0x20) ? KEY_MAIN : 0;
		ch[i].slot[MOD].key = main;
		ch[i].slot[CAR].key = main;
	}
	if (rythm_mode) {
		ch[6].slot[MOD].key |= (reg[0xbd] & 0x10) ? KEY_RHYTHM : 0; // BD1
		ch[6].slot[CAR].key |= (reg[0xbd] & 0x10) ? KEY_RHYTHM : 0; // BD2
		ch[7].slot[MOD].key |= (reg[0xbd] & 0x01) ? KEY_RHYTHM : 0; // HH
		ch[7].slot[CAR].key |= (reg[0xbd] & 0x08) ? KEY_RHYTHM : 0; // SD
		ch[8].slot[MOD].key |= (reg[0xbd] & 0x04) ? KEY_RHYTHM : 0; // TOM
		ch[8].slot[CAR].key |= (reg[0xbd] & 0x02) ? KEY_RHYTHM : 0; // CYM
	}
}


//
// Generate wave data
//

// Convert Amp(0 to EG_HEIGHT) to Phase(0 to 8PI).
static inline int wave2_8pi(int e)
{
	int shift = SLOT_AMP_BITS - PG_BITS - 2;
	return (shift > 0) ? (e >> shift) : (e << -shift);
}

unsigned Y8950::Slot::calc_phase(int lfo_pm)
{
	if (patch.PM) {
		phase += (dphase * lfo_pm) >> PM_AMP_BITS;
	} else {
		phase += dphase;
	}
	return phase >> DP_BASE_BITS;
}

#define S2E(x) Y8950::EnvPhaseIndex(int(x / EG_STEP))
static const Y8950::EnvPhaseIndex SL[16] = {
	S2E( 0), S2E( 3), S2E( 6), S2E( 9), S2E(12), S2E(15), S2E(18), S2E(21),
	S2E(24), S2E(27), S2E(30), S2E(33), S2E(36), S2E(39), S2E(42), S2E(93)
};
unsigned Y8950::Slot::calc_envelope(int lfo_am)
{
	unsigned egout = 0;
	switch (eg_mode) {
	case ATTACK:
		eg_phase += eg_dphase;
		if (eg_phase >= EG_DP_MAX) {
			egout = 0;
			eg_phase = Y8950::EnvPhaseIndex(0);
			eg_mode = DECAY;
			updateEG();
		} else {
			egout = AR_ADJUST_TABLE[eg_phase.toInt()];
		}
		break;

	case DECAY:
		eg_phase += eg_dphase;
		if (eg_phase >= SL[patch.SL]) {
			eg_phase = SL[patch.SL];
			eg_mode = SUSTAIN;
			updateEG();
		}
		egout = eg_phase.toInt();
		break;

	case SUSTAIN:
		if (!patch.EG) {
			eg_phase += eg_dphase;
		}
		egout = eg_phase.toInt();
		if (egout >= EG_MUTE) {
			eg_phase = EG_DP_MAX;
			eg_mode = FINISH;
			egout = EG_MUTE - 1;
		}
		break;

	case RELEASE:
		eg_phase += eg_dphase;
		egout = eg_phase.toInt();
		if (egout >= EG_MUTE) {
			eg_phase = EG_DP_MAX;
			eg_mode = FINISH;
			egout = EG_MUTE - 1;
		}
		break;

	case FINISH:
		egout = EG_MUTE - 1;
		break;
	}

	egout = ((egout + tll) * EG_PER_DB);
	if (patch.AM) {
		egout += lfo_am;
	}
	return std::min<unsigned>(egout, DB_MUTE - 1);
}

int Y8950::Slot::calc_slot_car(int lfo_pm, int lfo_am, int fm)
{
	unsigned egout = calc_envelope(lfo_am);
	int pgout = calc_phase(lfo_pm) + wave2_8pi(fm);
	return dB2LinTab[sintable[pgout & PG_MASK] + egout];
}

int Y8950::Slot::calc_slot_mod(int lfo_pm, int lfo_am)
{
	unsigned egout = calc_envelope(lfo_am);
	unsigned pgout = calc_phase(lfo_pm);

	if (patch.FB != 0) {
		pgout += wave2_8pi(feedback) >> patch.FB;
	}
	int newOutput = dB2LinTab[sintable[pgout & PG_MASK] + egout];
	feedback = (output + newOutput) >> 1;
	output = newOutput;
	return feedback;
}

int Y8950::Slot::calc_slot_tom(int lfo_pm, int lfo_am)
{
	unsigned egout = calc_envelope(lfo_am);
	unsigned pgout = calc_phase(lfo_pm);
	return dB2LinTab[sintable[pgout & PG_MASK] + egout];
}

int Y8950::Slot::calc_slot_snare(int lfo_pm, int lfo_am, int whitenoise)
{
	unsigned egout = calc_envelope(lfo_am);
	unsigned pgout = calc_phase(lfo_pm);
	unsigned tmp = (pgout & (1 << (PG_BITS - 1))) ? 0 : 2 * DB_MUTE;
	return (dB2LinTab[tmp + egout] + dB2LinTab[egout + whitenoise]) >> 1;
}

int Y8950::Slot::calc_slot_cym(int lfo_am, int a, int b)
{
	unsigned egout = calc_envelope(lfo_am);
	return (dB2LinTab[egout + a] + dB2LinTab[egout + b]) >> 1;
}

// HI-HAT
int Y8950::Slot::calc_slot_hat(int lfo_am, int a, int b, int whitenoise)
{
	unsigned egout = calc_envelope(lfo_am);
	return (dB2LinTab[egout + whitenoise] +
	        dB2LinTab[egout + a] +
	        dB2LinTab[egout + b]) >> 2;
}

int Y8950::getAmplificationFactor() const
{
	return 1 << (15 - DB2LIN_AMP_BITS);
}

void Y8950::setEnabled(bool enabled_, EmuTime::param time)
{
	updateStream(time);
	enabled = enabled_;
}

bool Y8950::checkMuteHelper()
{
	if (!enabled) {
		return true;
	}
	for (int i = 0; i < 6; ++i) {
		if (ch[i].slot[CAR].isActive()) return false;
	}
	if (!rythm_mode) {
		for(int i = 6; i < 9; ++i) {
			if (ch[i].slot[CAR].isActive()) return false;
		}
	} else {
		if (ch[6].slot[CAR].isActive()) return false;
		if (ch[7].slot[MOD].isActive()) return false;
		if (ch[7].slot[CAR].isActive()) return false;
		if (ch[8].slot[MOD].isActive()) return false;
		if (ch[8].slot[CAR].isActive()) return false;
	}

	return adpcm.isMuted();
}

void Y8950::generateChannels(int** bufs, unsigned num)
{
	// TODO implement per-channel mute (instead of all-or-nothing)
	if (checkMuteHelper()) {
		// TODO update internal state even when muted
		// during mute pm_phase, am_phase, noiseA_phase, noiseB_phase
		// and noise_seed aren't updated, probably ok
		for (int i = 0; i < 9 + 5 + 1; ++i) {
			bufs[i] = nullptr;
		}
		return;
	}

	for (unsigned sample = 0; sample < num; ++sample) {
		// Amplitude modulation: 27 output levels (triangle waveform);
		// 1 level takes one of: 192, 256 or 448 samples
		// One entry from LFO_AM_TABLE lasts for 64 samples
		// lfo_am_table is 210 elements long
		++am_phase;
		if (am_phase == (LFO_AM_TAB_ELEMENTS * 64)) am_phase = 0;
		unsigned tmp = lfo_am_table[am_phase / 64];
		int lfo_am = am_mode ? tmp : tmp / 4;

		pm_phase = (pm_phase + PM_DPHASE) & (PM_DP_WIDTH - 1);
		int lfo_pm = pmtable[pm_mode][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];

		if (noise_seed & 1) {
			noise_seed ^= 0x24000;
		}
		noise_seed >>= 1;
		int whitenoise = noise_seed & 1 ? DB_POS(6) : DB_NEG(6);

		noiseA_phase += noiseA_dphase;
		noiseA_phase &= (0x40 << 11) - 1;
		if ((noiseA_phase >> 11) == 0x3f) {
			noiseA_phase = 0;
		}
		int noiseA = noiseA_phase & (0x03 << 11) ? DB_POS(6) : DB_NEG(6);

		noiseB_phase += noiseB_dphase;
		noiseB_phase &= (0x10 << 11) - 1;
		int noiseB = noiseB_phase & (0x0A << 11) ? DB_POS(6) : DB_NEG(6);

		int m = rythm_mode ? 6 : 9;
		for (int i = 0; i < m; ++i) {
			if (ch[i].slot[CAR].isActive()) {
				bufs[i][sample] += ch[i].alg
					? ch[i].slot[CAR].calc_slot_car(lfo_pm, lfo_am, 0) +
					       ch[i].slot[MOD].calc_slot_mod(lfo_pm, lfo_am)
					: ch[i].slot[CAR].calc_slot_car(lfo_pm, lfo_am,
					       ch[i].slot[MOD].calc_slot_mod(lfo_pm, lfo_am));
			} else {
				//bufs[i][sample] += 0;
			}
		}
		if (rythm_mode) {
			//bufs[6][sample] += 0;
			//bufs[7][sample] += 0;
			//bufs[8][sample] += 0;

			// TODO wasn't in original source either
			ch[7].slot[MOD].calc_phase(lfo_pm);
			ch[8].slot[CAR].calc_phase(lfo_pm);

			bufs[ 9][sample] += (ch[6].slot[CAR].isActive())
				? 2 * ch[6].slot[CAR].calc_slot_car(lfo_pm, lfo_am,
						    ch[6].slot[MOD].calc_slot_mod(lfo_pm, lfo_am))
				: 0;
			bufs[10][sample] += (ch[7].slot[CAR].isActive())
				? 2 * ch[7].slot[CAR].calc_slot_snare(lfo_pm, lfo_am, whitenoise)
				: 0;
			bufs[11][sample] += (ch[8].slot[CAR].isActive())
				? 2 * ch[8].slot[CAR].calc_slot_cym(lfo_am, noiseA, noiseB)
				: 0;
			bufs[12][sample] += (ch[7].slot[MOD].isActive())
				? 2 * ch[7].slot[MOD].calc_slot_hat(lfo_am, noiseA, noiseB, whitenoise)
				: 0;
			bufs[13][sample] += (ch[8].slot[MOD].isActive())
				? 2 * ch[8].slot[MOD].calc_slot_tom(lfo_pm, lfo_am)
				: 0;
		} else {
			//bufs[ 9] += 0;
			//bufs[10] += 0;
			//bufs[11] += 0;
			//bufs[12] += 0;
			//bufs[13] += 0;
		}

		bufs[14][sample] += adpcm.calcSample();
	}
}

//
// I/O Ctrl
//

void Y8950::writeReg(byte rg, byte data, EmuTime::param time)
{
	int stbl[32] = {
		 0,  2,  4,  1,  3,  5, -1, -1,
		 6,  8, 10,  7,  9, 11, -1, -1,
		12, 14, 16, 13, 15, 17, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1
	};

	// TODO only for registers that influence sound
	// TODO also ADPCM
	//if (rg >= 0x20) {
		// update the output buffer before changing the register
		updateStream(time);
	//}

	switch (rg & 0xe0) {
	case 0x00: {
		switch (rg) {
		case 0x01: // TEST
			// TODO
			// Y8950 MSX-AUDIO Test register $01 (write only)
			//
			// Bit Description
			//
			//  7  Reset LFOs - seems to force the LFOs to their initial
			//     values (eg. maximum amplitude, zero phase deviation)
			//
			//  6  something to do with ADPCM - bit 0 of the status
			//     register is affected by setting this bit (PCM BSY)
			//
			//  5  No effect? - Waveform select enable in YM3812 OPL2 so seems
			//     reasonable that this bit wouldn't have been used in OPL
			//
			//  4  No effect?
			//
			//  3  Faster LFOs - increases the frequencies of the LFOs and
			//     (maybe) the timers (cf. YM2151 test register)
			//
			//  2  Reset phase generators - No phase generator output, but
			//     envelope generators still work (can hear a transient
			//     when they are gated)
			//
			//  1  No effect?
			//
			//  0  Reset envelopes - Envelope generator outputs forced
			//     to maximum, so all enabled voices sound at maximum
			reg[rg] = data;
			break;

		case 0x02: // TIMER1 (reso. 80us)
			timer1->setValue(data);
			reg[rg] = data;
			break;

		case 0x03: // TIMER2 (reso. 320us)
			timer2->setValue(data);
			reg[rg] = data;
			break;

		case 0x04: // FLAG CONTROL
			if (data & Y8950::R04_IRQ_RESET) {
				resetStatus(0x78);	// reset all flags
			} else {
				changeStatusMask((~data) & 0x78);
				timer1->setStart((data & Y8950::R04_ST1) != 0, time);
				timer2->setStart((data & Y8950::R04_ST2) != 0, time);
				reg[rg] = data;
			}
			adpcm.resetStatus();
			break;

		case 0x06: // (KEYBOARD OUT)
			connector.write(data, time);
			reg[rg] = data;
			break;

		case 0x07: // START/REC/MEM DATA/REPEAT/SP-OFF/-/-/RESET
			periphery.setSPOFF((data & 8) != 0, time); // bit 3
			// fall-through

		case 0x08: // CSM/KEY BOARD SPLIT/-/-/SAMPLE/DA AD/64K/ROM
		case 0x09: // START ADDRESS (L)
		case 0x0A: // START ADDRESS (H)
		case 0x0B: // STOP ADDRESS (L)
		case 0x0C: // STOP ADDRESS (H)
		case 0x0D: // PRESCALE (L)
		case 0x0E: // PRESCALE (H)
		case 0x0F: // ADPCM-DATA
		case 0x10: // DELTA-N (L)
		case 0x11: // DELTA-N (H)
		case 0x12: // ENVELOP CONTROL
		case 0x1A: // PCM-DATA
			reg[rg] = data;
			adpcm.writeReg(rg, data, time);
			break;

		case 0x15: // DAC-DATA  (bit9-2)
			reg[rg] = data;
			if (reg[0x08] & 0x04) {
				int tmp = static_cast<signed char>(reg[0x15]) * 256
				        + reg[0x16];
				tmp = (tmp * 4) >> (7 - reg[0x17]);
				tmp = Math::clipIntToShort(tmp);
				dac13.writeDAC(tmp, time);
			}
			break;
		case 0x16: //           (bit1-0)
			reg[rg] = data & 0xC0;
			break;
		case 0x17: //           (exponent)
			reg[rg] = data & 0x07;
			break;

		case 0x18: // I/O-CONTROL (bit3-0)
			// 0 -> input
			// 1 -> output
			reg[rg] = data;
			periphery.write(reg[0x18], reg[0x19], time);
			break;

		case 0x19: // I/O-DATA (bit3-0)
			reg[rg] = data;
			periphery.write(reg[0x18], reg[0x19], time);
			break;
		}
		break;
	}
	case 0x20: {
		int s = stbl[rg & 0x1f];
		if (s >= 0) {
			auto& chan = ch[s / 2];
			auto& slot = chan.slot[s & 1];
			slot.patch.AM = (data >> 7) &  1;
			slot.patch.PM = (data >> 6) &  1;
			slot.patch.EG = (data >> 5) &  1;
			slot.patch.setKeyScaleRate((data & 0x10) != 0);
			slot.patch.ML = (data >> 0) & 15;
			slot.updateAll(chan.freq);
		}
		reg[rg] = data;
		break;
	}
	case 0x40: {
		int s = stbl[rg & 0x1f];
		if (s >= 0) {
			auto& chan = ch[s / 2];
			auto& slot = chan.slot[s & 1];
			slot.patch.KL = (data >> 6) &  3;
			slot.patch.TL = (data >> 0) & 63;
			slot.updateAll(chan.freq);
		}
		reg[rg] = data;
		break;
	}
	case 0x60: {
		int s = stbl[rg & 0x1f];
		if (s >= 0) {
			auto& slot = ch[s / 2].slot[s & 1];
			slot.patch.AR = (data >> 4) & 15;
			slot.patch.DR = (data >> 0) & 15;
			slot.updateEG();
		}
		reg[rg] = data;
		break;
	}
	case 0x80: {
		int s = stbl[rg & 0x1f];
		if (s >= 0) {
			auto& slot = ch[s / 2].slot[s & 1];
			slot.patch.SL = (data >> 4) & 15;
			slot.patch.RR = (data >> 0) & 15;
			slot.updateEG();
		}
		reg[rg] = data;
		break;
	}
	case 0xa0: {
		if (rg == 0xbd) {
			am_mode = (data & 0x80) != 0;
			pm_mode = (data & 0x40) != 0;

			setRythmMode(data);
			if (rythm_mode) {
				if (data & 0x10) keyOn_BD();  else keyOff_BD();
				if (data & 0x08) keyOn_SD();  else keyOff_SD();
				if (data & 0x04) keyOn_TOM(); else keyOff_TOM();
				if (data & 0x02) keyOn_CYM(); else keyOff_CYM();
				if (data & 0x01) keyOn_HH();  else keyOff_HH();
			}
			ch[6].slot[MOD].updateAll(ch[6].freq);
			ch[6].slot[CAR].updateAll(ch[6].freq);
			ch[7].slot[MOD].updateAll(ch[7].freq);
			ch[7].slot[CAR].updateAll(ch[7].freq);
			ch[8].slot[MOD].updateAll(ch[8].freq);
			ch[8].slot[CAR].updateAll(ch[8].freq);

			reg[rg] = data;
			break;
		}
		unsigned c = rg & 0x0f;
		if (c > 8) {
			// 0xa9-0xaf 0xb9-0xbf
			break;
		}
		unsigned freq;
		if (!(rg & 0x10)) {
			// 0xa0-0xa8
			freq = data | ((reg[rg + 0x10] & 0x1F) << 8);
		} else {
			// 0xb0-0xb8
			if (data & 0x20) {
				ch[c].keyOn (KEY_MAIN);
			} else {
				ch[c].keyOff(KEY_MAIN);
			}
			freq = reg[rg - 0x10] | ((data & 0x1F) << 8);
		}
		ch[c].setFreq(freq);
		unsigned fNum  = freq % 1024;
		unsigned block = freq / 1024;
		switch (c) {
		case 7: noiseA_dphase = fNum << block;
			break;
		case 8: noiseB_dphase = fNum << block;
			break;
		}
		ch[c].slot[CAR].updateAll(freq);
		ch[c].slot[MOD].updateAll(freq);
		reg[rg] = data;
		break;
	}
	case 0xc0: {
		if (rg > 0xc8)
			break;
		int c = rg - 0xC0;
		ch[c].slot[MOD].patch.setFeedbackShift((data >> 1) & 7);
		ch[c].alg = data & 1;
		reg[rg] = data;
	}
	}
}

byte Y8950::readReg(byte rg, EmuTime::param time)
{
	updateStream(time); // TODO only when necessary

	byte result;
	switch (rg) {
		case 0x0F: // ADPCM-DATA
		case 0x13: //  ???
		case 0x14: //  ???
		case 0x1A: // PCM-DATA
			result = adpcm.readReg(rg, time);
			break;
		default:
			result = peekReg(rg, time);
	}
	return result;
}

byte Y8950::peekReg(byte rg, EmuTime::param time) const
{
	switch (rg) {
		case 0x05: // (KEYBOARD IN)
			return connector.peek(time);

		case 0x0F: // ADPCM-DATA
		case 0x13: //  ???
		case 0x14: //  ???
		case 0x1A: // PCM-DATA
			return adpcm.peekReg(rg, time);

		case 0x19: { // I/O DATA
			byte input = periphery.read(time);
			byte output = reg[0x19];
			byte enable = reg[0x18];
			return (output & enable) | (input & ~enable) | 0xF0;
		}
		default:
			return reg[rg];
	}
}

byte Y8950::readStatus(EmuTime::param time)
{
	byte result = peekStatus(time);
	//std::cout << "status: " << (int)result << std::endl;
	return result;
}

byte Y8950::peekStatus(EmuTime::param time) const
{
	const_cast<Y8950Adpcm&>(adpcm).sync(time);
	return (status & (0x80 | statusMask)) | 0x06; // bit 1 and 2 are always 1
}

void Y8950::callback(byte flag)
{
	setStatus(flag);
}

void Y8950::setStatus(byte flags)
{
	status |= flags;
	if (status & statusMask) {
		status |= 0x80;
		irq.set();
	}
}
void Y8950::resetStatus(byte flags)
{
	status &= ~flags;
	if (!(status & statusMask)) {
		status &= 0x7f;
		irq.reset();
	}
}
byte Y8950::peekRawStatus() const
{
	return status;
}
void Y8950::changeStatusMask(byte newMask)
{
	statusMask = newMask;
	status &= statusMask;
	if (status) {
		status |= 0x80;
		irq.set();
	} else {
		status &= 0x7f;
		irq.reset();
	}
}


template<typename Archive>
void Y8950::Patch::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("AM", AM);
	ar.serialize("PM", PM);
	ar.serialize("EG", EG);
	ar.serialize("KR", KR);
	ar.serialize("ML", ML);
	ar.serialize("KL", KL);
	ar.serialize("TL", TL);
	ar.serialize("FB", FB);
	ar.serialize("AR", AR);
	ar.serialize("DR", DR);
	ar.serialize("SL", SL);
	ar.serialize("RR", RR);
}

static std::initializer_list<enum_string<Y8950::EnvelopeState>> envelopeStateInfo = {
	{ "ATTACK",  Y8950::ATTACK  },
	{ "DECAY",   Y8950::DECAY   },
	{ "SUSTAIN", Y8950::SUSTAIN },
	{ "RELEASE", Y8950::RELEASE },
	{ "FINISH",  Y8950::FINISH  }
};
SERIALIZE_ENUM(Y8950::EnvelopeState, envelopeStateInfo);

// version 1: initial version
// version 2: 'slotStatus' is replaced with 'key' and no longer serialized
//            instead it's recalculated via update_key_status()
// version 3: serialize 'eg_mode' as an enum instead of an int, also merged
//            the 2 enum values SUSHOLD and SUSTINE into SUSTAIN
template<typename Archive>
void Y8950::Slot::serialize(Archive& ar, unsigned version)
{
	ar.serialize("feedback", feedback);
	ar.serialize("output", output);
	ar.serialize("phase", phase);
	ar.serialize("eg_phase", eg_phase);
	ar.serialize("patch", patch);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("eg_mode", eg_mode);
	} else {
		assert(ar.isLoader());
		int tmp = 0; // dummy init to avoid warning
		ar.serialize("eg_mode", tmp);
		switch (tmp) {
			case 0:  eg_mode = ATTACK;  break;
			case 1:  eg_mode = DECAY;   break;
			case 2:  eg_mode = SUSTAIN; break; // was SUSHOLD
			case 3:  eg_mode = SUSTAIN; break; // was SUSTINE
			case 4:  eg_mode = RELEASE; break;
			default: eg_mode = FINISH;  break;
		}
	}

	// These are restored by call to updateAll() in Y8950::Channel::serialize()
	//  dphase, tll, dphaseARTableRks, dphaseDRTableRks, eg_dphase
	// These are restored by update_key_status():
	//  key
}

template<typename Archive>
void Y8950::Channel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mod", slot[MOD]);
	ar.serialize("car", slot[CAR]);
	ar.serialize("freq", freq);
	ar.serialize("alg", alg);

	if (ar.isLoader()) {
		slot[MOD].updateAll(freq);
		slot[CAR].updateAll(freq);
	}
}

template<typename Archive>
void Y8950::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("keyboardConnector", connector);
	ar.serialize("adpcm", adpcm);
	ar.serialize("timer1", *timer1);
	ar.serialize("timer2", *timer2);
	ar.serialize("irq", irq);
	ar.serialize_blob("registers", reg, sizeof(reg));
	ar.serialize("pm_phase", pm_phase);
	ar.serialize("am_phase", am_phase);
	ar.serialize("noise_seed", noise_seed);
	ar.serialize("noiseA_phase", noiseA_phase);
	ar.serialize("noiseB_phase", noiseB_phase);
	ar.serialize("noiseA_dphase", noiseA_dphase);
	ar.serialize("noiseB_dphase", noiseB_dphase);
	ar.serialize("channels", ch);
	ar.serialize("status", status);
	ar.serialize("statusMask", statusMask);
	ar.serialize("rythm_mode", rythm_mode);
	ar.serialize("am_mode", am_mode);
	ar.serialize("pm_mode", pm_mode);
	ar.serialize("enabled", enabled);

	// TODO restore more state from registers
	static const byte rewriteRegs[] = {
		6,       // connector
		15,      // dac13
	};
	if (ar.isLoader()) {
		update_key_status();
		EmuTime::param time = motherBoard.getCurrentTime();
		for (auto r : rewriteRegs) {
			writeReg(r, reg[r], time);
		}
	}
}


// SimpleDebuggable

Y8950::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                              const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs", "MSX-AUDIO", 0x100)
{
}

byte Y8950::Debuggable::read(unsigned address, EmuTime::param time)
{
	auto& y8950 = OUTER(Y8950, debuggable);
	return y8950.peekReg(address, time);
}

void Y8950::Debuggable::write(unsigned address, byte value, EmuTime::param time)
{
	auto& y8950 = OUTER(Y8950, debuggable);
	y8950.writeReg(address, value, time);
}

INSTANTIATE_SERIALIZE_METHODS(Y8950);
SERIALIZE_CLASS_VERSION(Y8950::Slot, 3);

} // namespace openmsx
