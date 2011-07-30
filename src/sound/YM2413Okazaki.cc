// $Id$

/*
 * Based on:
 *    emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001
 * heavily rewritten to fit openMSX structure
 */

#include "YM2413Okazaki.hh"
#include "serialize.hh"
#include "inline.hh"
#include "unreachable.hh"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cassert>

namespace openmsx {

namespace YM2413Okazaki {

// Dynamic range (Accuracy of sin table)
static const int DB_BITS = 8;
static const int DB_MUTE = 1 << DB_BITS;

static const double DB_STEP = 48.0 / DB_MUTE;
static const double EG_STEP = 0.375;
static const double TL_STEP = 0.75;

// PM speed(Hz) and depth(cent)
static const double PM_SPEED = 6.4;
static const double PM_DEPTH = 13.75;

// Size of Sintable ( 8 -- 18 can be used, but 9 recommended.)
static const int PG_BITS = 9;
static const int PG_WIDTH = 1 << PG_BITS;
static const int PG_MASK = PG_WIDTH - 1;

// Phase increment counter
static const int DP_BITS = 18;
static const int DP_BASE_BITS = DP_BITS - PG_BITS;

// Dynamic range of envelope
static const int EG_BITS = 7;

// Bits for liner value
static const int DB2LIN_AMP_BITS = 8;
static const int SLOT_AMP_BITS = DB2LIN_AMP_BITS;

// Bits for Pitch and Amp modulator
static const int PM_PG_BITS = 8;
static const int PM_PG_WIDTH = 1 << PM_PG_BITS;
static const int PM_DP_BITS = 16;
static const int PM_DP_WIDTH = 1 << PM_DP_BITS;
static const int PM_DP_MASK = PM_DP_WIDTH - 1;
static const int AM_PG_BITS = 8;
static const int AM_PG_WIDTH = 1 << AM_PG_BITS;
static const int AM_DP_BITS = 16;
static const int AM_DP_WIDTH = 1 << AM_DP_BITS;
static const int AM_DP_MASK = AM_DP_WIDTH - 1;

// dB to linear table (used by Slot)
static const int DBTABLEN = 3 * DB_MUTE; // enough to not have to check for overflow
// indices in range:
//   [0,        DB_MUTE )    actual values, from max to min
//   [DB_MUTE,  DBTABLEN)    filled with min val (to allow some overflow in index)
//   [DBTABLEN, 2*DBTABLEN)  as above but for negative output values
static int dB2LinTab[DBTABLEN * 2];

// WaveTable for each envelope amp
//  values are in range [0, DB_MUTE)             (for positive values)
//                  or  [0, DB_MUTE) + DBTABLEN  (for negative values)
static unsigned fullsintable[PG_WIDTH];
static unsigned halfsintable[PG_WIDTH];
static unsigned* waveform[2] = {fullsintable, halfsintable};

// LFO Table
static PhaseModulation pmtable[PM_PG_WIDTH];

// LFO Amplitude Modulation table (verified on real YM3812)
// 27 output levels (triangle waveform);
// 1 level takes one of: 192, 256 or 448 samples
//
// Length: 210 elements.
//  Each of the elements has to be repeated
//  exactly 64 times (on 64 consecutive samples).
//  The whole table takes: 64 * 210 = 13440 samples.
//
// Verified on real YM3812 (OPL2), but I believe it's the same for YM2413
// because it closely matches the YM2413 AM parameters:
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

// Noise and LFO
static unsigned PM_DPHASE =
	unsigned(PM_SPEED * PM_DP_WIDTH / (YM2413Core::CLOCK_FREQ / 72.0));

// Liner to Log curve conversion table (for Attack rate).
static unsigned AR_ADJUST_TABLE[1 << EG_BITS];

// Phase incr table for attack, decay and release
//  note: original code had indices swapped. It also had
//        a separate table for attack.
static EnvPhaseIndex dphaseDRTable[16][16];

static const EnvPhaseIndex EG_DP_MAX = EnvPhaseIndex(1 << 7);
static const EnvPhaseIndex EG_DP_INF = EnvPhaseIndex(1 << 8); // as long as it's bigger

// KSL + TL Table   values are in range [0, 112)
static unsigned tllTable[16 * 8][4];


//
// Helper functions
//
static inline int EG2DB(int d)
{
	return d * int(EG_STEP / DB_STEP);
}

static inline int TL2EG(int d)
{
	return d * int(TL_STEP / EG_STEP);
}
static inline unsigned DB_POS(double x)
{
	int result = int(x / DB_STEP);
	assert(0 <= result);
	assert(result < DB_MUTE);
	return result;
}
static inline unsigned DB_NEG(double x)
{
	return DBTABLEN + DB_POS(x);
}

static inline bool BIT(unsigned s, unsigned b)
{
	return (s >> b) & 1;
}


//
// Create tables
//

// Table for AR to LogCurve.
static void makeAdjustTable()
{
	AR_ADJUST_TABLE[0] = (1 << EG_BITS) - 1;
	for (int i = 1; i < (1 << EG_BITS); ++i) {
		AR_ADJUST_TABLE[i] = unsigned(double(1 << EG_BITS) - 1 -
		         ((1 << EG_BITS) - 1) * ::log(double(i)) / ::log(127.0));
	}
}

// Table for dB(0 .. DB_MUTE-1) to lin(0 .. DB2LIN_AMP_WIDTH)
static void makeDB2LinTable()
{
	for (int i = 0; i < DB_MUTE; ++i) {
		dB2LinTab[i] = int(double((1 << DB2LIN_AMP_BITS) - 1) *
		                   pow(10, -double(i) * DB_STEP / 20));
	}
	dB2LinTab[DB_MUTE - 1] = 0;
	for (int i = DB_MUTE; i < DBTABLEN; ++i) {
		dB2LinTab[i] = 0;
	}
	for (int i = 0; i < DBTABLEN; ++i) {
		dB2LinTab[i + DBTABLEN] = -dB2LinTab[i];
	}
}

// lin(+0.0 .. +1.0) to dB(DB_MUTE-1 .. 0)
static int lin2db(double d)
{
	return (d == 0)
		? DB_MUTE - 1
		: std::min(-int(20.0 * log10(d) / DB_STEP), DB_MUTE - 1); // 0 - 127
}

// Sin Table
static void makeSinTable()
{
	for (int i = 0; i < PG_WIDTH / 4; ++i) {
		fullsintable[i] = lin2db(sin(2.0 * M_PI * i / PG_WIDTH));
	}
	for (int i = 0; i < PG_WIDTH / 4; ++i) {
		fullsintable[PG_WIDTH / 2 - 1 - i] = fullsintable[i];
	}
	for (int i = 0; i < PG_WIDTH / 2; ++i) {
		fullsintable[PG_WIDTH / 2 + i] = DBTABLEN + fullsintable[i];
	}

	for (int i = 0; i < PG_WIDTH / 2; ++i) {
		halfsintable[i] = fullsintable[i];
	}
	for (int i = PG_WIDTH / 2; i < PG_WIDTH; ++i) {
		halfsintable[i] = fullsintable[0];
	}
}

/**
 * Sawtooth function with amplitude 1 and period 1.
 */
static inline double saw(double phase)
{
	if (phase < 0.25) {
		return phase * 4.0;
	} else if (phase < 0.75) {
		return 2.0 - (phase * 4.0);
	} else {
		return -4.0 + (phase * 4.0);
	}
}

// Table for Pitch Modulator
static void makePmTable()
{
	for (int i = 0; i < PM_PG_WIDTH; ++i) {
		 pmtable[i] = PhaseModulation(pow(
			2, PM_DEPTH / 1200.0 * saw(i / double(PM_PG_WIDTH))));
	}
}

static void makeTllTable()
{
	// Processed version of Table III-5 from the Application Manual.
	static const unsigned kltable[16] = {
		0, 24, 32, 37, 40, 43, 45, 47, 48, 50, 51, 52, 53, 54, 55, 56
	};

	for (unsigned freq = 0; freq < 16 * 8; ++freq) {
		unsigned fnum = freq & 15;
		unsigned block = freq / 16;
		int tmp = 2 * kltable[fnum] - 16 * (7 - block);
		for (unsigned KL = 0; KL < 4; ++KL) {
			tllTable[freq][KL] = (tmp <= 0 || KL == 0) ? 0 : (tmp >> (3 - KL));
		}
	}
}

// Rate Table for Decay
static void makeDphaseDRTable()
{
	for (unsigned Rks = 0; Rks < 16; ++Rks) {
		dphaseDRTable[Rks][0] = EnvPhaseIndex(0);
		for (unsigned DR = 1; DR < 16; ++DR) {
			unsigned RM = std::min(DR + (Rks >> 2), 15u);
			unsigned RL = Rks & 3;
			dphaseDRTable[Rks][DR] =
				EnvPhaseIndex(RL + 4) >> (16 - RM);
		}
	}
}


//
// Patch
//
Patch::Patch()
	: AM(false), PM(false), EG(false)
	, ML(0), KL(0), TL(0), WF(0), AR(0), DR(0), SL(0), RR(0)
{
	setFeedbackShift(0);
	setKeyScaleRate(0);
}

void Patch::initModulator(const byte* data)
{
	AM = (data[0] >> 7) & 1;
	PM = (data[0] >> 6) & 1;
	EG = (data[0] >> 5) & 1;
	ML = (data[0] >> 0) & 15;
	KL = (data[2] >> 6) & 3;
	TL = (data[2] >> 0) & 63;
	WF = (data[3] >> 3) & 1;
	AR = (data[4] >> 4) & 15;
	DR = (data[4] >> 0) & 15;
	SL = (data[6] >> 4) & 15;
	RR = (data[6] >> 0) & 15;
	setFeedbackShift((data[3] >> 0) & 7);
	setKeyScaleRate((data[0] >> 4) & 1);
}

void Patch::initCarrier(const byte* data)
{
	AM = (data[1] >> 7) & 1;
	PM = (data[1] >> 6) & 1;
	EG = (data[1] >> 5) & 1;
	ML = (data[1] >> 0) & 15;
	KL = (data[3] >> 6) & 3;
	TL = 0;
	WF = (data[3] >> 4) & 1;
	AR = (data[5] >> 4) & 15;
	DR = (data[5] >> 0) & 15;
	SL = (data[7] >> 4) & 15;
	RR = (data[7] >> 0) & 15;
	setFeedbackShift(0);
	setKeyScaleRate((data[1] >> 4) & 1);
}

void Patch::setFeedbackShift(byte value)
{
	FB = value ? 8 - value : 0;
}

void Patch::setKeyScaleRate(bool value)
{
	KR = value ? 8 : 10;
}


//
// Slot
//
void Slot::reset()
{
	sintbl = waveform[0];
	cphase = 0;
	dphase = 0;
	output = 0;
	feedback = 0;
	setEnvelopeState(FINISH);
	dphaseDRTableRks = dphaseDRTable[0];
	tll = 0;
	sustain = false;
	volume = 0;
	slot_on_flag = 0;
}

void Slot::updatePG(unsigned freq)
{
	static const unsigned mltable[16] = {
		1,   1*2,  2*2,  3*2,  4*2,  5*2,  6*2,  7*2,
		8*2, 9*2, 10*2, 10*2, 12*2, 12*2, 15*2, 15*2
	};

	unsigned fnum = freq & 511;
	unsigned block = freq / 512;
	dphase = ((fnum * mltable[patch.ML]) << block) >> (20 - DP_BITS);
}

void Slot::updateTLL(unsigned freq, bool actAsCarrier)
{
	tll = tllTable[freq >> 5][patch.KL] + TL2EG(actAsCarrier ? volume : patch.TL);
}

void Slot::updateRKS(unsigned freq)
{
	unsigned rks = freq >> patch.KR;
	assert(rks < 16);
	dphaseDRTableRks = dphaseDRTable[rks];
}

void Slot::updateWF()
{
	sintbl = waveform[patch.WF];
}

void Slot::updateEG()
{
	switch (state) {
	case ATTACK:
		// Original code had separate table for AR, the values in
		// this table were 12 times bigger than the values in the
		// dphaseDRTableRks table, expect for the value for AR=15.
		// But when AR=15, the value doesn't matter.
		//
		// This factor 12 can also be seen in the attack/decay rates
		// table in the ym2413 application manual (table III-7, page
		// 13). For other chips like OPL1, OPL3 this ratio seems to be
		// different.
		eg_dphase = dphaseDRTableRks[patch.AR] * 12;
		break;
	case DECAY:
		eg_dphase = dphaseDRTableRks[patch.DR];
		break;
	case SUSTAIN:
		eg_dphase = dphaseDRTableRks[patch.RR];
		break;
	case RELEASE: {
		unsigned idx = sustain ? 5
		                       : (patch.EG ? patch.RR
		                                   : 7);
		eg_dphase = dphaseDRTableRks[idx];
		break;
	}
	case SETTLE:
		// Value based on ym2413 application manual:
		//  - p10: (envelope graph)
		//         DP: 10ms
		//  - p13: (table III-7 attack and decay rates)
		//         Rate 12-0 -> 10.22ms
		//         Rate 12-1 ->  8.21ms
		//         Rate 12-2 ->  6.84ms
		//         Rate 12-3 ->  5.87ms
		// The datasheet doesn't say anything about key-scaling for
		// this state (in fact it doesn't say much at all about this
		// state). Experiments showed that the with key-scaling the
		// output matches closer the real HW. Also all other states use
		// key-scaling.
		eg_dphase = dphaseDRTableRks[12];
		break;
	case SUSHOLD:
	case FINISH:
		eg_dphase = EnvPhaseIndex(0);
		break;
	}
}

void Slot::updateAll(unsigned freq, bool actAsCarrier)
{
	updatePG(freq);
	updateTLL(freq, actAsCarrier);
	updateRKS(freq);
	updateWF();
	updateEG(); // EG should be updated last
}

#define S2E(x) EnvPhaseIndex(int(x / EG_STEP))
static const EnvPhaseIndex SL[16] = {
	S2E( 0.0), S2E( 3.0), S2E( 6.0), S2E( 9.0),
	S2E(12.0), S2E(15.0), S2E(18.0), S2E(21.0),
	S2E(24.0), S2E(27.0), S2E(30.0), S2E(33.0),
	S2E(36.0), S2E(39.0), S2E(42.0), S2E(48.0)
};
#undef S2E

void Slot::setEnvelopeState(EnvelopeState state_)
{
	state = state_;
	switch (state) {
	case ATTACK:
		eg_phase_max = (patch.AR == 15) ? EnvPhaseIndex(0) : EG_DP_MAX;
		break;
	case DECAY:
		eg_phase_max = SL[patch.SL];
		break;
	case SUSHOLD:
		eg_phase_max = EG_DP_INF;
		break;
	case SETTLE:
	case SUSTAIN:
	case RELEASE:
		eg_phase_max = EG_DP_MAX;
		break;
	case FINISH:
		eg_phase = EG_DP_MAX;
		eg_phase_max = EG_DP_INF;
		break;
	}
	updateEG();
}

bool Slot::isActive() const
{
	return state != FINISH;
}

// Slot key on
void Slot::slotOn()
{
	setEnvelopeState(ATTACK);
	eg_phase = EnvPhaseIndex(0);
	cphase = 0;
}

// Slot key on, without resetting the phase
void Slot::slotOn2()
{
	setEnvelopeState(ATTACK);
	eg_phase = EnvPhaseIndex(0);
}

// Slot key off
void Slot::slotOff()
{
	if (state == FINISH) return; // already in off state
	if (state == ATTACK) {
		eg_phase = EnvPhaseIndex(AR_ADJUST_TABLE[eg_phase.toInt()]);
	}
	setEnvelopeState(RELEASE);
}


// Change a rhythm voice
void Slot::setPatch(Patch& patch)
{
	this->patch = patch; // copy data
}

void Slot::setVolume(unsigned newVolume)
{
	volume = newVolume;
}


//
// Channel
//

Channel::Channel()
{
	car.sibling = &mod; // car needs a pointer to its sibling
	mod.sibling = NULL; // mod doesn't need this pointer
}

void Channel::reset(YM2413& ym2413)
{
	freq = 0;
	mod.reset();
	car.reset();
	setPatch(0, ym2413);
}

// Change a voice
void Channel::setPatch(unsigned num, YM2413& ym2413)
{
	patch_number = num;
	mod.setPatch(ym2413.getPatch(num, false));
	car.setPatch(ym2413.getPatch(num, true));
	patchFlags = ( car.patch.AM       << 0) |
	             ( car.patch.PM       << 1) |
	             ( mod.patch.AM       << 2) |
	             ( mod.patch.PM       << 3) |
	             ((mod.patch.FB != 0) << 4);
}

// Set sustain parameter
void Channel::setSustain(bool sustain, bool modActAsCarrier)
{
	car.sustain = sustain;
	if (modActAsCarrier) {
		mod.sustain = sustain;
	}
}

// Volume : 6bit ( Volume register << 2 )
void Channel::setVol(unsigned volume)
{
	car.setVolume(volume);
}

// set Frequency (combined fnum (=9bit) and block (=3bit))
void Channel::setFreq(unsigned freq_)
{
	freq = freq_;
}

// Channel key on
void Channel::keyOn()
{
	// TODO Should we also test mod.slot_on_flag?
	//      Should we    set    mod.slot_on_flag?
	//      Can make a difference for channel 7/8 in ryhthm mode.
	if (!car.slot_on_flag) {
		car.setEnvelopeState(SETTLE);
		// this will shortly set both car and mod to ATTACK state
	}
	car.slot_on_flag |= 1;
	mod.slot_on_flag |= 1;
}

// Channel key off
void Channel::keyOff()
{
	// Note: no mod.slotOff() in original code!!!
	if (car.slot_on_flag) {
		car.slot_on_flag &= ~1;
		mod.slot_on_flag &= ~1;
		if (!car.slot_on_flag) {
			car.slotOff();
		}
	}
}


//
// YM2413
//

static byte inst_data[16 + 3][8] = {
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, // user instrument
	{ 0x61,0x61,0x1e,0x17,0xf0,0x7f,0x00,0x17 }, // violin
	{ 0x13,0x41,0x16,0x0e,0xfd,0xf4,0x23,0x23 }, // guitar
	{ 0x03,0x01,0x9a,0x04,0xf3,0xf3,0x13,0xf3 }, // piano
	{ 0x11,0x61,0x0e,0x07,0xfa,0x64,0x70,0x17 }, // flute
	{ 0x22,0x21,0x1e,0x06,0xf0,0x76,0x00,0x28 }, // clarinet
	{ 0x21,0x22,0x16,0x05,0xf0,0x71,0x00,0x18 }, // oboe
	{ 0x21,0x61,0x1d,0x07,0x82,0x80,0x17,0x17 }, // trumpet
	{ 0x23,0x21,0x2d,0x16,0x90,0x90,0x00,0x07 }, // organ
	{ 0x21,0x21,0x1b,0x06,0x64,0x65,0x10,0x17 }, // horn
	{ 0x21,0x21,0x0b,0x1a,0x85,0xa0,0x70,0x07 }, // synthesizer
	{ 0x23,0x01,0x83,0x10,0xff,0xb4,0x10,0xf4 }, // harpsichord
	{ 0x97,0xc1,0x20,0x07,0xff,0xf4,0x22,0x22 }, // vibraphone
	{ 0x61,0x00,0x0c,0x05,0xc2,0xf6,0x40,0x44 }, // synthesizer bass
	{ 0x01,0x01,0x56,0x03,0x94,0xc2,0x03,0x12 }, // acoustic bass
	{ 0x21,0x01,0x89,0x03,0xf1,0xe4,0xf0,0x23 }, // electric guitar
	{ 0x07,0x21,0x14,0x00,0xee,0xf8,0xff,0xf8 },
	{ 0x01,0x31,0x00,0x00,0xf8,0xf7,0xf8,0xf7 },
	{ 0x25,0x11,0x00,0x00,0xf8,0xfa,0xf8,0x55 }
};

YM2413::YM2413()
{
	memset(reg, 0, sizeof(reg)); // avoid UMR

	for (unsigned i = 0; i < 16 + 3; ++i) {
		patches[i][0].initModulator(inst_data[i]);
		patches[i][1].initCarrier(inst_data[i]);
	}
	makePmTable();
	makeDB2LinTable();
	makeAdjustTable();
	makeTllTable();
	makeSinTable();
	makeDphaseDRTable();

	reset();
}

// Reset whole of OPLL except patch datas
void YM2413::reset()
{
	pm_phase = 0;
	am_phase = 0;
	noise_seed = 0xFFFF;
	idleSamples = 0;

	for (unsigned i = 0; i < 9; ++i) {
		channels[i].reset(*this);
	}
	for (unsigned i = 0; i < 0x40; ++i) {
		writeReg(i, 0);
	}
}

// Drum key on
void YM2413::keyOn_BD()
{
	Channel& ch6 = channels[6];
	if (!ch6.car.slot_on_flag) {
		ch6.car.setEnvelopeState(SETTLE);
		// this will shortly set both car and mod to ATTACK state
	}
	ch6.car.slot_on_flag |= 2;
	ch6.mod.slot_on_flag |= 2;
}
void YM2413::keyOn_HH()
{
	// TODO do these also use the SETTLE stuff?
	Channel& ch7 = channels[7];
	if (!ch7.mod.slot_on_flag) {
		ch7.mod.slotOn2();
	}
	ch7.mod.slot_on_flag |= 2;
}
void YM2413::keyOn_SD()
{
	Channel& ch7 = channels[7];
	if (!ch7.car.slot_on_flag) {
		ch7.car.slotOn();
	}
	ch7.car.slot_on_flag |= 2;
}
void YM2413::keyOn_TOM()
{
	Channel& ch8 = channels[8];
	if (!ch8.mod.slot_on_flag) {
		ch8.mod.slotOn();
	}
	ch8.mod.slot_on_flag |= 2;
}
void YM2413::keyOn_CYM()
{
	Channel& ch8 = channels[8];
	if (!ch8.car.slot_on_flag) {
		ch8.car.slotOn2();
	}
	ch8.car.slot_on_flag |= 2;
}

// Drum key off
void YM2413::keyOff_BD()
{
	Channel& ch6 = channels[6];
	if (ch6.car.slot_on_flag) {
		ch6.car.slot_on_flag &= ~2;
		ch6.mod.slot_on_flag &= ~2;
		if (!ch6.car.slot_on_flag) {
			ch6.car.slotOff();
		}
	}
}
void YM2413::keyOff_HH()
{
	Channel& ch7 = channels[7];
	if (ch7.mod.slot_on_flag) {
		ch7.mod.slot_on_flag &= ~2;
		if (ch7.mod.slot_on_flag) {
			ch7.mod.slotOff();
		}
	}
}
void YM2413::keyOff_SD()
{
	Channel& ch7 = channels[7];
	if (ch7.car.slot_on_flag) {
		ch7.car.slot_on_flag &= ~2;
		if (!ch7.car.slot_on_flag) {
			ch7.car.slotOff();
		}
	}
}
void YM2413::keyOff_TOM()
{
	Channel& ch8 = channels[8];
	if (ch8.mod.slot_on_flag) {
		ch8.mod.slot_on_flag &= ~2;
		if (!ch8.mod.slot_on_flag) {
			ch8.mod.slotOff();
		}
	}
}
void YM2413::keyOff_CYM()
{
	Channel& ch8 = channels[8];
	if (ch8.car.slot_on_flag) {
		ch8.car.slot_on_flag &= ~2;
		if (!ch8.car.slot_on_flag) {
			ch8.car.slotOff();
		}
	}
}

void YM2413::update_rhythm_mode()
{
	Channel& ch6 = channels[6];
	Channel& ch7 = channels[7];
	Channel& ch8 = channels[8];
	assert((ch6.patch_number & 0x10) == (ch7.patch_number & 0x10));
	assert((ch6.patch_number & 0x10) == (ch8.patch_number & 0x10));
	if (ch6.patch_number & 0x10) {
		if (!isRhythm()) {
			// ON -> OFF
			ch6.setPatch(reg[0x36] >> 4, *this);
			keyOff_BD();
			ch7.setPatch(reg[0x37] >> 4, *this);
			keyOff_SD();
			keyOff_HH();
			ch8.setPatch(reg[0x38] >> 4, *this);
			keyOff_TOM();
			keyOff_CYM();
		}
	} else if (isRhythm()) {
		// OFF -> ON
		ch6.mod.setEnvelopeState(FINISH);
		ch6.car.setEnvelopeState(FINISH);
		ch6.setPatch(16, *this);
		ch7.mod.setEnvelopeState(FINISH);
		ch7.car.setEnvelopeState(FINISH);
		ch7.setPatch(17, *this);
		ch7.mod.setVolume((reg[0x37] >> 4) << 2);
		ch8.mod.setEnvelopeState(FINISH);
		ch8.car.setEnvelopeState(FINISH);
		ch8.setPatch(18, *this);
		ch8.mod.setVolume((reg[0x38] >> 4) << 2);
	}
}

void YM2413::update_key_status()
{
	for (unsigned i = 0; i < 9; ++i) {
		int slot_on = (reg[0x20 + i] & 0x10) ? 1 : 0;
		Channel& ch = channels[i];
		ch.mod.slot_on_flag = slot_on;
		ch.car.slot_on_flag = slot_on;
	}
	if (isRhythm()) {
		Channel& ch6 = channels[6];
		ch6.mod.slot_on_flag |= (reg[0x0e] & 0x10) ? 2 : 0; // BD1
		ch6.car.slot_on_flag |= (reg[0x0e] & 0x10) ? 2 : 0; // BD2
		Channel& ch7 = channels[7];
		ch7.mod.slot_on_flag |= (reg[0x0e] & 0x01) ? 2 : 0; // HH
		ch7.car.slot_on_flag |= (reg[0x0e] & 0x08) ? 2 : 0; // SD
		Channel& ch8 = channels[8];
		ch8.mod.slot_on_flag |= (reg[0x0e] & 0x04) ? 2 : 0; // TOM
		ch8.car.slot_on_flag |= (reg[0x0e] & 0x02) ? 2 : 0; // CYM
	}
}

// Convert Amp(0 to EG_HEIGHT) to Phase(0 to 8PI)
static inline int wave2_8pi(int e)
{
	int shift = SLOT_AMP_BITS - PG_BITS - 2;
	if (shift > 0) {
		return e >> shift;
	} else {
		return e << -shift;
	}
}

// PG
template <bool HAS_PM>
ALWAYS_INLINE unsigned Slot::calc_phase(PhaseModulation lfo_pm)
{
	assert(patch.PM == HAS_PM);
	if (HAS_PM) {
		cphase += (lfo_pm * dphase).toInt();
	} else {
		cphase += dphase;
	}
	return cphase >> DP_BASE_BITS;
}

// EG
void Slot::calc_envelope_outline(unsigned& out)
{
	switch (state) {
	case ATTACK:
		out = 0;
		eg_phase = EnvPhaseIndex(0);
		setEnvelopeState(DECAY);
		break;
	case DECAY:
		eg_phase = eg_phase_max;
		setEnvelopeState(patch.EG ? SUSHOLD : SUSTAIN);
		break;
	case SUSTAIN:
	case RELEASE:
		setEnvelopeState(FINISH);
		break;
	case SETTLE:
		// Comment copied from Burczynski code (didn't verify myself):
		//   When CARRIER envelope gets down to zero level, phases in
		//   BOTH operators are reset (at the same time?).
		slotOn();
		sibling->slotOn();
		break;
	case SUSHOLD:
	case FINISH:
	default:
		UNREACHABLE;
		break;
	}
}
template <bool HAS_AM, bool FIXED_ENV>
ALWAYS_INLINE unsigned Slot::calc_envelope(int lfo_am, unsigned fixed_env)
{
	assert(patch.AM == HAS_AM);
	assert(!FIXED_ENV || (state == SUSHOLD) || (state == FINISH));

	if (FIXED_ENV) {
		unsigned out = fixed_env;
		if (HAS_AM) {
			out += lfo_am; // [0, 512)
			out |= 3;
		} else {
			// out |= 3   is already done in calc_fixed_env()
		}
		return out;
	} else {
		unsigned out = eg_phase.toInt(); // in range [0, 128)
		if (state == ATTACK) {
			out = AR_ADJUST_TABLE[out]; // [0, 128)
		}
		eg_phase += eg_dphase;
		if (eg_phase >= eg_phase_max) {
			calc_envelope_outline(out);
		}
		out = EG2DB(out + tll); // [0, 480)
		if (HAS_AM) {
			out += lfo_am; // [0, 512)
		}
		return out | 3;
	}
}
template <bool HAS_AM> unsigned Slot::calc_fixed_env() const
{
	assert((state == SUSHOLD) || (state == FINISH));
	assert(eg_dphase == EnvPhaseIndex(0));
	unsigned out = eg_phase.toInt(); // in range [0, 128)
	out = EG2DB(out + tll); // [0, 480)
	if (!HAS_AM) {
		out |= 3;
	}
	return out;
}

// CARRIER
template <bool HAS_PM, bool HAS_AM, bool FIXED_ENV>
ALWAYS_INLINE int Slot::calc_slot_car(PhaseModulation lfo_pm, int lfo_am, int fm, unsigned fixed_env)
{
	int phase = calc_phase<HAS_PM>(lfo_pm) + wave2_8pi(fm);
	unsigned egout = calc_envelope<HAS_AM, FIXED_ENV>(lfo_am, fixed_env);
	int newOutput = dB2LinTab[sintbl[phase & PG_MASK] + egout];
	output = (output + newOutput) >> 1;
	return output;
}

// MODULATOR
template <bool HAS_PM, bool HAS_AM, bool HAS_FB, bool FIXED_ENV>
ALWAYS_INLINE int Slot::calc_slot_mod(PhaseModulation lfo_pm, int lfo_am, unsigned fixed_env)
{
	assert((patch.FB != 0) == HAS_FB);
	unsigned phase = calc_phase<HAS_PM>(lfo_pm);
	unsigned egout = calc_envelope<HAS_AM, FIXED_ENV>(lfo_am, fixed_env);
	if (HAS_FB) {
		phase += wave2_8pi(feedback) >> patch.FB;
	}
	int newOutput = dB2LinTab[sintbl[phase & PG_MASK] + egout];
	feedback = (output + newOutput) >> 1;
	output = newOutput;
	return feedback;
}

// TOM (ch8 mod)
ALWAYS_INLINE int Slot::calc_slot_tom()
{
	unsigned phase = calc_phase<false>(PhaseModulation());
	unsigned egout = calc_envelope<false, false>(0, 0);
	return dB2LinTab[sintbl[phase & PG_MASK] + egout];
}

// SNARE (ch7 car)
ALWAYS_INLINE int Slot::calc_slot_snare(bool noise)
{
	unsigned phase = calc_phase<false>(PhaseModulation());
	unsigned egout = calc_envelope<false, false>(0, 0);
	return BIT(phase, 7)
		? dB2LinTab[(noise ? DB_POS(0.0) : DB_POS(15.0)) + egout]
		: dB2LinTab[(noise ? DB_NEG(0.0) : DB_NEG(15.0)) + egout];
}

// TOP-CYM (ch8 car)
ALWAYS_INLINE int Slot::calc_slot_cym(unsigned phase7, unsigned phase8)
{
	unsigned egout = calc_envelope<false, false>(0, 0);
	unsigned dbout = (((BIT(phase7, PG_BITS - 8) ^
	                    BIT(phase7, PG_BITS - 1)) |
	                   BIT(phase7, PG_BITS - 7)) ^
	                  ( BIT(phase8, PG_BITS - 7) &
	                   !BIT(phase8, PG_BITS - 5)))
	               ? DB_NEG(3.0)
	               : DB_POS(3.0);
	return dB2LinTab[dbout + egout];
}

// HI-HAT (ch7 mod)
ALWAYS_INLINE int Slot::calc_slot_hat(unsigned phase7, unsigned phase8, bool noise)
{
	unsigned egout = calc_envelope<false, false>(0, 0);
	unsigned dbout = (((BIT(phase7, PG_BITS - 8) ^
	                    BIT(phase7, PG_BITS - 1)) |
	                   BIT(phase7, PG_BITS - 7)) ^
	                  ( BIT(phase8, PG_BITS - 7) &
	                   !BIT(phase8, PG_BITS - 5)))
	               ? (noise ? DB_NEG(12.0) : DB_NEG(24.0))
	               : (noise ? DB_POS(12.0) : DB_POS(24.0));
	return dB2LinTab[dbout + egout];
}

int YM2413::getAmplificationFactor() const
{
	return 1 << (15 - DB2LIN_AMP_BITS);
}

bool YM2413::isRhythm() const
{
	return (reg[0x0E] & 0x20) != 0;
}

Patch& YM2413::getPatch(unsigned instrument, bool carrier)
{
	return patches[instrument][carrier];
}

template <unsigned FLAGS>
ALWAYS_INLINE void YM2413::calcChannel(Channel& ch, int* buf, unsigned num)
{
	// VC++ requires explicit conversion to bool. Compiler bug??
	const bool HAS_CAR_AM = (FLAGS &  1) != 0;
	const bool HAS_CAR_PM = (FLAGS &  2) != 0;
	const bool HAS_MOD_AM = (FLAGS &  4) != 0;
	const bool HAS_MOD_PM = (FLAGS &  8) != 0;
	const bool HAS_MOD_FB = (FLAGS & 16) != 0;
	const bool HAS_CAR_FIXED_ENV = (FLAGS & 32) != 0;
	const bool HAS_MOD_FIXED_ENV = (FLAGS & 64) != 0;

	unsigned tmp_pm_phase = pm_phase;
	unsigned tmp_am_phase = am_phase;
	unsigned car_fixed_env = 0; // dummy
	unsigned mod_fixed_env = 0; // dummy
	if (HAS_CAR_FIXED_ENV) {
		car_fixed_env = ch.car.calc_fixed_env<HAS_CAR_AM>();
	}
	if (HAS_MOD_FIXED_ENV) {
		mod_fixed_env = ch.mod.calc_fixed_env<HAS_MOD_AM>();
	}

	unsigned sample = 0;
	do {
		PhaseModulation lfo_pm;
		if (HAS_CAR_PM || HAS_MOD_PM) {
			tmp_pm_phase = (tmp_pm_phase + PM_DPHASE) & PM_DP_MASK;
			lfo_pm = pmtable[tmp_pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
		}
		int lfo_am = 0; // avoid warning
		if (HAS_CAR_AM || HAS_MOD_AM) {
			++tmp_am_phase;
			if (tmp_am_phase == (LFO_AM_TAB_ELEMENTS * 64)) {
				tmp_am_phase = 0;
			}
			lfo_am = lfo_am_table[tmp_am_phase / 64];
		}
		int fm = ch.mod.calc_slot_mod<HAS_MOD_PM, HAS_MOD_AM, HAS_MOD_FB, HAS_MOD_FIXED_ENV>(
		                      lfo_pm, lfo_am, mod_fixed_env);
		buf[sample] += ch.car.calc_slot_car<HAS_CAR_PM, HAS_CAR_AM, HAS_CAR_FIXED_ENV>(
		                      lfo_pm, lfo_am, fm, car_fixed_env);
		++sample;
	} while (sample < num);
}

void YM2413::generateChannels(int* bufs[9 + 5], unsigned num)
{
	assert(num != 0);

	// TODO make channelActiveBits a member and
	//      keep it up-to-date all the time

	// bits 0-8  -> ch[0-8].car
	// bits 9-17 -> ch[0-8].mod (only ch7 and ch8 are used)
	unsigned channelActiveBits = 0;

	unsigned m = isRhythm() ? 6 : 9;
	for (unsigned ch = 0; ch < m; ++ch) {
		if (channels[ch].car.isActive()) {
			channelActiveBits |= 1 << ch;
		} else {
			bufs[ch] = 0;
		}
	}

	if (isRhythm()) {
		bufs[6] = 0;
		bufs[7] = 0;
		bufs[8] = 0;
		for (unsigned ch = 6; ch < 9; ++ch) {
			if (channels[ch].car.isActive()) {
				channelActiveBits |= 1 << ch;
			} else {
				bufs[ch + 3] = 0;
			}
		}
		if (channels[7].mod.isActive()) {
			channelActiveBits |= 1 << (7 + 9);
		} else {
			bufs[12] = 0;
		}
		if (channels[8].mod.isActive()) {
			channelActiveBits |= 1 << (8 + 9);
		} else {
			bufs[13] = 0;
		}
	} else {
		bufs[ 9] = 0;
		bufs[10] = 0;
		bufs[11] = 0;
		bufs[12] = 0;
		bufs[13] = 0;
	}

	if (channelActiveBits) {
		idleSamples = 0;
	} else {
		if (idleSamples > (CLOCK_FREQ / (72 * 5))) {
			// Optimization:
			//   idle for over 1/5s = 200ms
			//   we don't care that noise / AM / PM isn't exactly
			//   in sync with the real HW when music resumes
			// Alternative:
			//   implement an efficient advance(n) method
			return;
		}
		idleSamples += num;
	}

	for (unsigned i = 0; i < m; ++i) {
		if (channelActiveBits & (1 << i)) {
			// below we choose between 32 specialized versions of
			// calcChannel() this allows to move a lot of
			// conditional code out of the inner-loop
			Channel& ch = channels[i];
			bool carFixedEnv = (ch.car.state == SUSHOLD) ||
			                   (ch.car.state == FINISH);
			bool modFixedEnv = (ch.mod.state == SUSHOLD) ||
			                   (ch.mod.state == FINISH);
			if (ch.car.state == SETTLE) {
				modFixedEnv = false;
			}
			unsigned flags = ch.patchFlags |
			                 (carFixedEnv ? 32 : 0) |
			                 (modFixedEnv ? 64 : 0);
			switch (flags) {
			case   0: calcChannel<  0>(ch, bufs[i], num); break;
			case   1: calcChannel<  1>(ch, bufs[i], num); break;
			case   2: calcChannel<  2>(ch, bufs[i], num); break;
			case   3: calcChannel<  3>(ch, bufs[i], num); break;
			case   4: calcChannel<  4>(ch, bufs[i], num); break;
			case   5: calcChannel<  5>(ch, bufs[i], num); break;
			case   6: calcChannel<  6>(ch, bufs[i], num); break;
			case   7: calcChannel<  7>(ch, bufs[i], num); break;
			case   8: calcChannel<  8>(ch, bufs[i], num); break;
			case   9: calcChannel<  9>(ch, bufs[i], num); break;
			case  10: calcChannel< 10>(ch, bufs[i], num); break;
			case  11: calcChannel< 11>(ch, bufs[i], num); break;
			case  12: calcChannel< 12>(ch, bufs[i], num); break;
			case  13: calcChannel< 13>(ch, bufs[i], num); break;
			case  14: calcChannel< 14>(ch, bufs[i], num); break;
			case  15: calcChannel< 15>(ch, bufs[i], num); break;
			case  16: calcChannel< 16>(ch, bufs[i], num); break;
			case  17: calcChannel< 17>(ch, bufs[i], num); break;
			case  18: calcChannel< 18>(ch, bufs[i], num); break;
			case  19: calcChannel< 19>(ch, bufs[i], num); break;
			case  20: calcChannel< 20>(ch, bufs[i], num); break;
			case  21: calcChannel< 21>(ch, bufs[i], num); break;
			case  22: calcChannel< 22>(ch, bufs[i], num); break;
			case  23: calcChannel< 23>(ch, bufs[i], num); break;
			case  24: calcChannel< 24>(ch, bufs[i], num); break;
			case  25: calcChannel< 25>(ch, bufs[i], num); break;
			case  26: calcChannel< 26>(ch, bufs[i], num); break;
			case  27: calcChannel< 27>(ch, bufs[i], num); break;
			case  28: calcChannel< 28>(ch, bufs[i], num); break;
			case  29: calcChannel< 29>(ch, bufs[i], num); break;
			case  30: calcChannel< 30>(ch, bufs[i], num); break;
			case  31: calcChannel< 31>(ch, bufs[i], num); break;
			case  32: calcChannel< 32>(ch, bufs[i], num); break;
			case  33: calcChannel< 33>(ch, bufs[i], num); break;
			case  34: calcChannel< 34>(ch, bufs[i], num); break;
			case  35: calcChannel< 35>(ch, bufs[i], num); break;
			case  36: calcChannel< 36>(ch, bufs[i], num); break;
			case  37: calcChannel< 37>(ch, bufs[i], num); break;
			case  38: calcChannel< 38>(ch, bufs[i], num); break;
			case  39: calcChannel< 39>(ch, bufs[i], num); break;
			case  40: calcChannel< 40>(ch, bufs[i], num); break;
			case  41: calcChannel< 41>(ch, bufs[i], num); break;
			case  42: calcChannel< 42>(ch, bufs[i], num); break;
			case  43: calcChannel< 43>(ch, bufs[i], num); break;
			case  44: calcChannel< 44>(ch, bufs[i], num); break;
			case  45: calcChannel< 45>(ch, bufs[i], num); break;
			case  46: calcChannel< 46>(ch, bufs[i], num); break;
			case  47: calcChannel< 47>(ch, bufs[i], num); break;
			case  48: calcChannel< 48>(ch, bufs[i], num); break;
			case  49: calcChannel< 49>(ch, bufs[i], num); break;
			case  50: calcChannel< 50>(ch, bufs[i], num); break;
			case  51: calcChannel< 51>(ch, bufs[i], num); break;
			case  52: calcChannel< 52>(ch, bufs[i], num); break;
			case  53: calcChannel< 53>(ch, bufs[i], num); break;
			case  54: calcChannel< 54>(ch, bufs[i], num); break;
			case  55: calcChannel< 55>(ch, bufs[i], num); break;
			case  56: calcChannel< 56>(ch, bufs[i], num); break;
			case  57: calcChannel< 57>(ch, bufs[i], num); break;
			case  58: calcChannel< 58>(ch, bufs[i], num); break;
			case  59: calcChannel< 59>(ch, bufs[i], num); break;
			case  60: calcChannel< 60>(ch, bufs[i], num); break;
			case  61: calcChannel< 61>(ch, bufs[i], num); break;
			case  62: calcChannel< 62>(ch, bufs[i], num); break;
			case  63: calcChannel< 63>(ch, bufs[i], num); break;
			case  64: calcChannel< 64>(ch, bufs[i], num); break;
			case  65: calcChannel< 65>(ch, bufs[i], num); break;
			case  66: calcChannel< 66>(ch, bufs[i], num); break;
			case  67: calcChannel< 67>(ch, bufs[i], num); break;
			case  68: calcChannel< 68>(ch, bufs[i], num); break;
			case  69: calcChannel< 69>(ch, bufs[i], num); break;
			case  70: calcChannel< 70>(ch, bufs[i], num); break;
			case  71: calcChannel< 71>(ch, bufs[i], num); break;
			case  72: calcChannel< 72>(ch, bufs[i], num); break;
			case  73: calcChannel< 73>(ch, bufs[i], num); break;
			case  74: calcChannel< 74>(ch, bufs[i], num); break;
			case  75: calcChannel< 75>(ch, bufs[i], num); break;
			case  76: calcChannel< 76>(ch, bufs[i], num); break;
			case  77: calcChannel< 77>(ch, bufs[i], num); break;
			case  78: calcChannel< 78>(ch, bufs[i], num); break;
			case  79: calcChannel< 79>(ch, bufs[i], num); break;
			case  80: calcChannel< 80>(ch, bufs[i], num); break;
			case  81: calcChannel< 81>(ch, bufs[i], num); break;
			case  82: calcChannel< 82>(ch, bufs[i], num); break;
			case  83: calcChannel< 83>(ch, bufs[i], num); break;
			case  84: calcChannel< 84>(ch, bufs[i], num); break;
			case  85: calcChannel< 85>(ch, bufs[i], num); break;
			case  86: calcChannel< 86>(ch, bufs[i], num); break;
			case  87: calcChannel< 87>(ch, bufs[i], num); break;
			case  88: calcChannel< 88>(ch, bufs[i], num); break;
			case  89: calcChannel< 89>(ch, bufs[i], num); break;
			case  90: calcChannel< 90>(ch, bufs[i], num); break;
			case  91: calcChannel< 91>(ch, bufs[i], num); break;
			case  92: calcChannel< 92>(ch, bufs[i], num); break;
			case  93: calcChannel< 93>(ch, bufs[i], num); break;
			case  94: calcChannel< 94>(ch, bufs[i], num); break;
			case  95: calcChannel< 95>(ch, bufs[i], num); break;
			case  96: calcChannel< 96>(ch, bufs[i], num); break;
			case  97: calcChannel< 97>(ch, bufs[i], num); break;
			case  98: calcChannel< 98>(ch, bufs[i], num); break;
			case  99: calcChannel< 99>(ch, bufs[i], num); break;
			case 100: calcChannel<100>(ch, bufs[i], num); break;
			case 101: calcChannel<101>(ch, bufs[i], num); break;
			case 102: calcChannel<102>(ch, bufs[i], num); break;
			case 103: calcChannel<103>(ch, bufs[i], num); break;
			case 104: calcChannel<104>(ch, bufs[i], num); break;
			case 105: calcChannel<105>(ch, bufs[i], num); break;
			case 106: calcChannel<106>(ch, bufs[i], num); break;
			case 107: calcChannel<107>(ch, bufs[i], num); break;
			case 108: calcChannel<108>(ch, bufs[i], num); break;
			case 109: calcChannel<109>(ch, bufs[i], num); break;
			case 110: calcChannel<110>(ch, bufs[i], num); break;
			case 111: calcChannel<111>(ch, bufs[i], num); break;
			case 112: calcChannel<112>(ch, bufs[i], num); break;
			case 113: calcChannel<113>(ch, bufs[i], num); break;
			case 114: calcChannel<114>(ch, bufs[i], num); break;
			case 115: calcChannel<115>(ch, bufs[i], num); break;
			case 116: calcChannel<116>(ch, bufs[i], num); break;
			case 117: calcChannel<117>(ch, bufs[i], num); break;
			case 118: calcChannel<118>(ch, bufs[i], num); break;
			case 119: calcChannel<119>(ch, bufs[i], num); break;
			case 120: calcChannel<120>(ch, bufs[i], num); break;
			case 121: calcChannel<121>(ch, bufs[i], num); break;
			case 122: calcChannel<122>(ch, bufs[i], num); break;
			case 123: calcChannel<123>(ch, bufs[i], num); break;
			case 124: calcChannel<124>(ch, bufs[i], num); break;
			case 125: calcChannel<125>(ch, bufs[i], num); break;
			case 126: calcChannel<126>(ch, bufs[i], num); break;
			case 127: calcChannel<127>(ch, bufs[i], num); break;
			default: UNREACHABLE;
			}
		}
	}
	// update AM, PM unit
	pm_phase += num * PM_DPHASE;
	am_phase = (am_phase + num) % (LFO_AM_TAB_ELEMENTS * 64);

	if (isRhythm()) {
		if (channelActiveBits & (1 << 6)) {
			Channel& ch6 = channels[6];
			for (unsigned sample = 0; sample < num; ++sample) {
				bufs[ 9][sample] += 2 *
				    ch6.car.calc_slot_car<false, false, false>(
				        PhaseModulation(), 0, ch6.mod.calc_slot_mod<
				                false, false, false, false>(PhaseModulation(), 0, 0), 0);
			}
		}
		Channel& ch7 = channels[7];
		Channel& ch8 = channels[8];
		unsigned old_noise = noise_seed;
		if (channelActiveBits & (1 << 7)) {
			for (unsigned sample = 0; sample < num; ++sample) {
				noise_seed >>= 1;
				bool noise_bit = noise_seed & 1;
				if (noise_bit) noise_seed ^= 0x8003020;
				bufs[10][sample] +=
					-2 * ch7.car.calc_slot_snare(noise_bit);
			}
		}
		unsigned old_cphase7 = ch7.mod.cphase;
		unsigned old_cphase8 = ch8.car.cphase;
		if (channelActiveBits & (1 << 8)) {
			for (unsigned sample = 0; sample < num; ++sample) {
				unsigned phase7 = ch7.mod.calc_phase<false>(PhaseModulation());
				unsigned phase8 = ch8.car.calc_phase<false>(PhaseModulation());
				bufs[11][sample] +=
					-2 * ch8.car.calc_slot_cym(phase7, phase8);
			}
		}
		if (channelActiveBits & (1 << (7 + 9))) {
			// restore noise, ch7/8 cphase
			noise_seed = old_noise;
			ch7.mod.cphase = old_cphase7;
			ch8.car.cphase = old_cphase8;
			for (unsigned sample = 0; sample < num; ++sample) {
				noise_seed >>= 1;
				bool noise_bit = noise_seed & 1;
				if (noise_bit) noise_seed ^= 0x8003020;
				unsigned phase7 = ch7.mod.calc_phase<false>(PhaseModulation());
				unsigned phase8 = ch8.car.calc_phase<false>(PhaseModulation());
				bufs[12][sample] +=
					2 * ch7.mod.calc_slot_hat(phase7, phase8, noise_bit);
			}
		}
		if (channelActiveBits & (1 << (8 + 9))) {
			for (unsigned sample = 0; sample < num; ++sample) {
				bufs[13][sample] += 2 * ch8.mod.calc_slot_tom();
			}
		}
	}
}

void YM2413::writeReg(byte regis, byte data)
{
	assert(regis < 0x40);
	reg[regis] = data;

	switch (regis) {
	case 0x00:
		patches[0][0].AM = (data >> 7) & 1;
		patches[0][0].PM = (data >> 6) & 1;
		patches[0][0].EG = (data >> 5) & 1;
		patches[0][0].setKeyScaleRate((data >> 4) & 1);
		patches[0][0].ML = (data >> 0) & 15;
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				if ((ch.mod.state == SUSHOLD) &&
				    (ch.mod.patch.EG == 0)) {
					ch.mod.setEnvelopeState(SUSTAIN);
				}
				ch.mod.updatePG(ch.freq);
				ch.mod.updateRKS(ch.freq);
				ch.mod.updateEG();
			}
		}
		break;
	case 0x01:
		patches[0][1].AM = (data >> 7) & 1;
		patches[0][1].PM = (data >> 6) & 1;
		patches[0][1].EG = (data >> 5) & 1;
		patches[0][1].setKeyScaleRate((data >> 4) & 1);
		patches[0][1].ML = (data >> 0) & 15;
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if(ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				if ((ch.car.state == SUSHOLD) &&
				    (ch.car.patch.EG == 0)) {
					ch.car.setEnvelopeState(SUSTAIN);
				}
				ch.car.updatePG(ch.freq);
				ch.car.updateRKS(ch.freq);
				ch.car.updateEG();
			}
		}
		break;
	case 0x02:
		patches[0][0].KL = (data >> 6) & 3;
		patches[0][0].TL = (data >> 0) & 63;
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				bool actAsCarrier = (i >= 7) && isRhythm();
				ch.mod.updateTLL(ch.freq, actAsCarrier);
			}
		}
		break;
	case 0x03:
		patches[0][1].KL = (data >> 6) & 3;
		patches[0][1].WF = (data >> 4) & 1;
		patches[0][0].WF = (data >> 3) & 1;
		patches[0][0].setFeedbackShift((data >> 0) & 7);
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				ch.mod.updateWF();
				ch.car.updateWF();
			}
		}
		break;
	case 0x04:
		patches[0][0].AR = (data >> 4) & 15;
		patches[0][0].DR = (data >> 0) & 15;
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				ch.mod.updateEG();
				if (ch.mod.state == ATTACK) {
					ch.mod.setEnvelopeState(ATTACK);
				}
			}
		}
		break;
	case 0x05:
		patches[0][1].AR = (data >> 4) & 15;
		patches[0][1].DR = (data >> 0) & 15;
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				ch.car.updateEG();
				if (ch.car.state == ATTACK) {
					ch.car.setEnvelopeState(ATTACK);
				}
			}
		}
		break;
	case 0x06:
		patches[0][0].SL = (data >> 4) & 15;
		patches[0][0].RR = (data >> 0) & 15;
		for (unsigned i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				ch.mod.updateEG();
				if (ch.mod.state == DECAY) {
					ch.mod.setEnvelopeState(DECAY);
				}
			}
		}
		break;
	case 0x07:
		patches[0][1].SL = (data >> 4) & 15;
		patches[0][1].RR = (data >> 0) & 15;
		for (unsigned i = 0; i < 9; i++) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0, *this); // TODO optimize
				ch.car.updateEG();
				if (ch.car.state == DECAY) {
					ch.car.setEnvelopeState(DECAY);
				}
			}
		}
		break;
	case 0x0e:
	{
		update_rhythm_mode();
		if (data & 0x20) {
			if (data & 0x10) keyOn_BD();  else keyOff_BD();
			if (data & 0x08) keyOn_SD();  else keyOff_SD();
			if (data & 0x04) keyOn_TOM(); else keyOff_TOM();
			if (data & 0x02) keyOn_CYM(); else keyOff_CYM();
			if (data & 0x01) keyOn_HH();  else keyOff_HH();
		}

		Channel& ch6 = channels[6];
		ch6.mod.updateAll(ch6.freq, false);
		ch6.car.updateAll(ch6.freq, true);
		Channel& ch7 = channels[7];
		ch7.mod.updateAll(ch7.freq, isRhythm());
		ch7.car.updateAll(ch7.freq, true);
		Channel& ch8 = channels[8];
		ch8.mod.updateAll(ch8.freq, isRhythm());
		ch8.car.updateAll(ch8.freq, true);
		break;
	}
	case 0x10:  case 0x11:  case 0x12:  case 0x13:
	case 0x14:  case 0x15:  case 0x16:  case 0x17:
	case 0x18:
	{
		unsigned cha = regis & 0x0F;
		Channel& ch = channels[cha];
		ch.setFreq((reg[0x20 + cha] & 0xF) << 8 | data);
		bool actAsCarrier = (cha >= 7) && isRhythm();
		ch.mod.updateAll(ch.freq, actAsCarrier);
		ch.car.updateAll(ch.freq, true);
		break;
	}
	case 0x20:  case 0x21:  case 0x22:  case 0x23:
	case 0x24:  case 0x25:  case 0x26:  case 0x27:
	case 0x28:
	{
		unsigned cha = regis & 0x0F;
		Channel& ch = channels[cha];
		ch.setFreq((data & 0xF) << 8 | reg[0x10 + cha]);
		bool modActAsCarrier = (cha >= 7) && isRhythm();
		ch.setSustain((data >> 5) & 1, modActAsCarrier);
		if (data & 0x10) {
			ch.keyOn();
		} else {
			ch.keyOff();
		}
		ch.mod.updateAll(ch.freq, modActAsCarrier);
		ch.car.updateAll(ch.freq, true);
		break;
	}
	case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
	case 0x35: case 0x36: case 0x37: case 0x38:
	{
		unsigned cha = regis & 0x0F;
		Channel& ch = channels[cha];
		unsigned j = (data >> 4) & 15;
		unsigned v = data & 15;
		if (isRhythm() && (cha >= 6)) {
			if (cha > 6) {
				// channel 7 or 8 in ryhthm mode
				channels[cha].mod.setVolume(j << 2);
			}
		} else {
			ch.setPatch(j, *this);
		}
		ch.setVol(v << 2);
		bool actAsCarrier = (cha >= 7) && isRhythm();
		ch.mod.updateAll(ch.freq, actAsCarrier);
		ch.car.updateAll(ch.freq, true);
		break;
	}
	default:
		break;
	}
}

byte YM2413::peekReg(byte r) const
{
	return reg[r];
}

} // namespace YM2413Okazaki

static enum_string<YM2413Okazaki::EnvelopeState> envelopeStateInfo[] = {
	{ "ATTACK",  YM2413Okazaki::ATTACK  },
	{ "DECAY",   YM2413Okazaki::DECAY   },
	{ "SUSHOLD", YM2413Okazaki::SUSHOLD },
	{ "SUSTAIN", YM2413Okazaki::SUSTAIN },
	{ "RELEASE", YM2413Okazaki::RELEASE },
	{ "SETTLE",  YM2413Okazaki::SETTLE  },
	{ "FINISH",  YM2413Okazaki::FINISH  }
};
SERIALIZE_ENUM(YM2413Okazaki::EnvelopeState, envelopeStateInfo);

namespace YM2413Okazaki {

template<typename Archive>
void Patch::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("AM", AM);
	ar.serialize("PM", PM);
	ar.serialize("EG", EG);
	ar.serialize("KR", KR);
	ar.serialize("ML", ML);
	ar.serialize("KL", KL);
	ar.serialize("TL", TL);
	ar.serialize("FB", FB);
	ar.serialize("WF", WF);
	ar.serialize("AR", AR);
	ar.serialize("DR", DR);
	ar.serialize("SL", SL);
	ar.serialize("RR", RR);
}

// version 1:  initial version
// version 2:  don't serialize "type / actAsCarrier" anymore, it's now
//             a calculated value
// version 3: don't serialize slot_on_flag anymore
template<typename Archive>
void Slot::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("feedback", feedback);
	ar.serialize("output", output);
	ar.serialize("cphase", cphase);
	ar.serialize("volume", volume);
	ar.serialize("state", state);
	ar.serialize("eg_phase", eg_phase);
	ar.serialize("sustain", sustain);

	// These are restored by call to updateAll() in YM2413::serialize()
	//   eg_dphase, dphaseDRTableRks, tll, dphase, sintbl
	// and by setEnvelopeState()
	//   eg_phase_max
	// and by setPatch()
	//   patch
	// and by update_key_status()
	//   slot_on_flag
}

template<typename Archive>
void Channel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mod", mod);
	ar.serialize("car", car);
	ar.serialize("patch_number", patch_number);
	ar.serialize("freq", freq);

	// These are restored by call to setPatch() in YM2413::serialize()
	//   patchFlags
}


// version 1:  initial version
// version 2:  'registers' are moved here (no longer serialized in base class)
template<typename Archive>
void YM2413::serialize(Archive& ar, unsigned version)
{
	if (ar.versionBelow(version, 2)) ar.beginTag("YM2413Core");
	ar.serialize("registers", reg);
	if (ar.versionBelow(version, 2)) ar.endTag("YM2413Core");

	// no need to serialize patches[1-19]
	ar.serialize("user_patch_mod", patches[0][0]);
	ar.serialize("user_patch_car", patches[0][1]);
	ar.serialize("channels", channels);
	ar.serialize("pm_phase", pm_phase);
	ar.serialize("am_phase", am_phase);
	ar.serialize("noise_seed", noise_seed);
	// don't serialize idleSamples, is only an optimization

	if (ar.isLoader()) {
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			ch.setPatch(ch.patch_number, *this); // before updateAll()
			bool actAsCarrier = (i >= 7) && isRhythm();
			ch.mod.updateAll(ch.freq, actAsCarrier);
			ch.car.updateAll(ch.freq, true);
			ch.mod.setEnvelopeState(ch.mod.state);
			ch.car.setEnvelopeState(ch.car.state);
		}
		update_key_status();
	}
}

} // namespace YM2413Okazaki

using YM2413Okazaki::YM2413;
INSTANTIATE_SERIALIZE_METHODS(YM2413);
REGISTER_POLYMORPHIC_INITIALIZER(YM2413Core, YM2413, "YM2413-Okazaki");

} // namespace openmsx
