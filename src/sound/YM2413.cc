// $Id$

/*
 * Based on:
 *    emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001
 * heavily rewritten to fit openMSX structure
 */

#include "YM2413.hh"
#include "YM2413Core.hh"
#include "FixedPoint.hh"
#include "noncopyable.hh"
#include <cmath>
#include <cassert>
#include <algorithm>

namespace openmsx {

namespace YM2413Okazaki {

typedef FixedPoint<8> PhaseModulation;
typedef FixedPoint<15> EnvPhaseIndex;
static const EnvPhaseIndex EG_DP_MAX = EnvPhaseIndex(1 << 7);

enum EnvelopeState {
	ATTACK, DECAY, SUSHOLD, SUSTAIN, RELEASE, SETTLE, FINISH
	};

class Patch {
public:
	/**
	 * Creates an uninitialized Patch object; call init() before use.
	 * This approach makes it possible to create an array of Patches.
	 */
	Patch();

	void initModulator(const byte* data);
	void initCarrier(const byte* data);

	/**
	 * Sets the amount of feedback [0..7].
	 */
	void setFeedbackShift(byte value) {
		FB = value ? 8 - value : 0;
	}

	bool AM, PM, EG;
	byte KR; // 0-1
	byte ML; // 0-15
	byte KL; // 0-3
	byte TL; // 0-63
	byte FB; // 0-7
	byte WF; // 0-1
	byte AR; // 0-15
	byte DR; // 0-15
	byte SL; // 0-15
	byte RR; // 0-15
};

class Slot {
public:
	void reset(bool type);

	inline void setEnvelopeState(EnvelopeState state);
	inline bool isActive() const;

	inline void slotOn();
	inline void slotOn2();
	inline void slotOff();
	inline void setPatch(Patch& patch);
	inline void setVolume(int volume);
	inline void calc_phase(PhaseModulation lfo_pm);
	inline void calc_envelope(int lfo_am);
	inline int calc_slot_car(int fm);
	inline int calc_slot_mod();
	inline int calc_slot_tom();
	inline int calc_slot_snare(bool noise);
	inline int calc_slot_cym(unsigned cphase_hh);
	inline int calc_slot_hat(unsigned cphase_cym, bool noise);
	inline void updatePG();
	inline void updateTLL();
	inline void updateRKS();
	inline void updateWF();
	inline void updateEG();
	inline void updateAll();

	Patch patch;
	unsigned* sintbl;	// Wavetable (for PG)

	// OUTPUT
	int feedback;
	int output;		// Output value of slot

	// for Phase Generator (PG)
	unsigned cphase;	// Phase counter
	unsigned dphase;	// Phase increment

	// for Envelope Generator (EG)
	int fnum;		// F-Number
	int block;		// Block
	int volume;		// Current volume
	int tll;		// Total Level + Key scale level
	int rks;		// Key scale offset (Rks)
	EnvelopeState state;	// Current state
	EnvPhaseIndex eg_phase;	// Phase
	EnvPhaseIndex eg_dphase;	// Phase increment amount
	unsigned egout;		// output
	bool sustain;	// Sustain
	bool type;		// 0 : modulator 1 : carrier
	bool slot_on_flag;
};

class Channel {
public:
	/**
	 * Initializes those parts that cannot be initialized in the constructor,
	 * because the constructor cannot have arguments since we want to create
	 * an array of Channels.
	 * This method should be called once, as soon as possible after
	 * construction.
	 */
	void init(Global& global);

	void reset();
	inline void setPatch(int num);
	inline void setSustain(bool sustain);
	inline void setVol(int volume);
	inline void setFnumber(int fnum);
	inline void setBlock(int block);
	inline void keyOn();
	inline void keyOff();

	Slot mod, car;
	int patch_number;

private:
	Global* global;
};

class Global : public YM2413Core {
public:
	Global(MSXMotherBoard& motherBoard, const std::string& name,
               const XMLElement& config, const EmuTime& time);
	virtual ~Global();

	void reset(const EmuTime& time);
	virtual void writeReg(byte reg, byte value, const EmuTime& time);

	bool isRhythm() {
		return reg[0x0E] & 0x20;
	}

	inline void keyOn_BD();
	inline void keyOn_SD();
	inline void keyOn_TOM();
	inline void keyOn_HH();
	inline void keyOn_CYM();
	inline void keyOff_BD();
	inline void keyOff_SD();
	inline void keyOff_TOM();
	inline void keyOff_HH();
	inline void keyOff_CYM();
	inline void update_rhythm_mode();
	inline void update_key_status();

	/**
	 * Generate output samples for each channel.
	 */
	virtual void generateChannels(int** bufs, unsigned num);

	Patch& getPatch(int instrument, bool carrier) {
		return patches[instrument][carrier];
	}

private:
	// Voice Data
	Patch patches[19][2];

	// Channel & Slot
	Channel channels[9];

	// Pitch Modulator
	unsigned pm_phase;

	// Amp Modulator
	unsigned am_phase;

	// Noise Generator
	int noise_seed;

	// Number of samples the output was completely silent
	unsigned idleSamples;
};


// Dynamic range (Accuracy of sin table)
static const int DB_BITS = 8;
static const int DB_MUTE = 1 << DB_BITS;

static const double DB_STEP = 48.0 / DB_MUTE;
static const double EG_STEP = 0.375;
static const double TL_STEP = 0.75;
static const double SL_STEP = 3.0;

// PM speed(Hz) and depth(cent)
static const double PM_SPEED = 6.4;
static const double PM_DEPTH = 13.75;

// AM speed(Hz) and depth(dB)
static const double AM_SPEED = 3.6413;
static const double AM_DEPTH = 4.875;

// Size of Sintable ( 8 -- 18 can be used, but 9 recommended.)
static const int PG_BITS = 9;
static const int PG_WIDTH = 1 << PG_BITS;
static const int PG_MASK = PG_WIDTH - 1;

// Phase increment counter
static const int DP_BITS = 18;
static const int DP_BASE_BITS = DP_BITS - PG_BITS;

// Dynamic range of envelope
static const int EG_BITS = 7;

// Dynamic range of total level
static const int TL_BITS = 6;

// Bits for liner value
static const int DB2LIN_AMP_BITS = 8;
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

// dB to linear table (used by Slot)
static int dB2LinTab[(DB_MUTE + DB_MUTE) * 2];

// WaveTable for each envelope amp
//  values are in range[0,   DB_MUTE)           (for positive values)
//                  or [2*DB_MUTE, 3*DB_MUTE)   (for negative values)
static unsigned fullsintable[PG_WIDTH];
static unsigned halfsintable[PG_WIDTH];
static unsigned* waveform[2] = {fullsintable, halfsintable};

// LFO Table
static PhaseModulation pmtable[PM_PG_WIDTH];
static int amtable[AM_PG_WIDTH];

// Noise and LFO
static unsigned PM_DPHASE =
	unsigned(PM_SPEED * PM_DP_WIDTH / (YM2413Core::CLOCK_FREQ / 72.0));
static unsigned AM_DPHASE =
	unsigned(AM_SPEED * AM_DP_WIDTH / (YM2413Core::CLOCK_FREQ / 72.0));

// Liner to Log curve conversion table (for Attack rate).
static unsigned AR_ADJUST_TABLE[1 << EG_BITS];

// Phase incr table for Attack
static EnvPhaseIndex dphaseARTable[16][16];
// Phase incr table for Decay and Release
static EnvPhaseIndex dphaseDRTable[16][16];

// KSL + TL Table
static unsigned tllTable[16][8][1 << TL_BITS][4];
static int rksTable[2][8][2];

// Phase incr table for PG
static unsigned dphaseTable[512][8][16];


//***************************************************//
//                                                   //
//  Helper functions                                 //
//                                                   //
//***************************************************//

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
	assert(0 <= x);
	assert(x < DB_MUTE);
	return result;
}
static inline unsigned DB_NEG(double x)
{
	return 2 * DB_MUTE + DB_POS(x);
}

// Cut the lower b bit off
template <typename T>
static inline T HIGHBITS(T c, int b)
{
	return c >> b;
}

static inline bool BIT(int s, int b)
{
	return (s >> b) & 1;
}


//***************************************************//
//                                                   //
//                  Create tables                    //
//                                                   //
//***************************************************//

// Table for AR to LogCurve.
static void makeAdjustTable()
{
	AR_ADJUST_TABLE[0] = (1 << EG_BITS) - 1;
	for (int i = 1; i < (1 << EG_BITS); ++i) {
		AR_ADJUST_TABLE[i] = unsigned(double(1 << EG_BITS) - 1 -
		                       ((1 << EG_BITS) - 1) * ::log(i) / ::log(127));
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
	for (int i = DB_MUTE; i < 2 * DB_MUTE; ++i) {
		dB2LinTab[i] = 0;
	}
	for (int i = 0; i < 2 * DB_MUTE; ++i) {
		dB2LinTab[i + 2 * DB_MUTE] = -dB2LinTab[i];
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
		fullsintable[PG_WIDTH / 2 + i] = 2 * DB_MUTE + fullsintable[i];
	}

	for (int i = 0; i < PG_WIDTH / 2; ++i) {
		halfsintable[i] = fullsintable[i];
	}
	for (int i = PG_WIDTH / 2; i < PG_WIDTH; ++i) {
		halfsintable[i] = fullsintable[0];
	}
}

/**
 * Sawtooth function with amplitude 1 and period 2Pi.
 */
static inline double saw(double phase)
{
	if (phase <= (M_PI / 2)) {
		return phase * 2 / M_PI;
	} else if (phase <= (M_PI * 3 / 2)) {
		return 2.0 - (phase * 2 / M_PI);
	} else {
		return -4.0 + phase * 2 / M_PI;
	}
}

// Table for Pitch Modulator
static void makePmTable()
{
	for (int i = 0; i < PM_PG_WIDTH; ++i) {
		 pmtable[i] = PhaseModulation(pow(
			2, double(PM_DEPTH) * saw(2.0 * M_PI * i / PM_PG_WIDTH) / 1200
			));
	}
}

// Table for Amp Modulator
static void makeAmTable()
{
	for (int i = 0; i < AM_PG_WIDTH; ++i) {
		amtable[i] = int(double(AM_DEPTH) / 2 / DB_STEP *
		                 (1.0 + saw(2.0 * M_PI * i / AM_PG_WIDTH)));
	}
}

// Phase increment counter table
static void makeDphaseTable()
{
	unsigned mltable[16] = {
		1,   1*2,  2*2,  3*2,  4*2,  5*2,  6*2,  7*2,
		8*2, 9*2, 10*2, 10*2, 12*2, 12*2, 15*2, 15*2
	};

	for (unsigned fnum = 0; fnum < 512; ++fnum) {
		for (unsigned block = 0; block < 8; ++block) {
			for (unsigned ML = 0; ML < 16; ++ML) {
				dphaseTable[fnum][block][ML] =
					((fnum * mltable[ML]) << block) >> (20 - DP_BITS);
			}
		}
	}
}

static void makeTllTable()
{
	double kltable[16] = {
		( 0.000 * 2), ( 9.000 * 2), (12.000 * 2), (13.875 * 2),
		(15.000 * 2), (16.125 * 2), (16.875 * 2), (17.625 * 2),
		(18.000 * 2), (18.750 * 2), (19.125 * 2), (19.500 * 2),
		(19.875 * 2), (20.250 * 2), (20.625 * 2), (21.000 * 2)
	};

	for (int fnum = 0; fnum < 16; ++fnum) {
		for (int block = 0; block < 8; ++block) {
			for (int TL = 0; TL < 64; ++TL) {
				tllTable[fnum][block][TL][0] = TL2EG(TL);
				for (int KL = 1; KL < 4; ++KL) {
					int tmp = int(
						kltable[fnum] - (3.000 * 2) * (7 - block));
					tllTable[fnum][block][TL][KL] =
						(tmp <= 0) ?
						TL2EG(TL) :
						unsigned((tmp >> (3 - KL)) / EG_STEP) + TL2EG(TL);
				}
			}
		}
	}
}

// Rate Table for Attack
static void makeDphaseARTable()
{
	for (int AR = 0; AR < 16; ++AR) {
		for (int Rks = 0; Rks < 16; ++Rks) {
			switch (AR) {
			case 0:
				dphaseARTable[AR][Rks] = EnvPhaseIndex(0);
				break;
			case 15:
				dphaseARTable[AR][Rks] = EnvPhaseIndex(0); // EG_DP_MAX
				break;
			default: {
				int RM = std::min(AR + (Rks >> 2), 15);
				int RL = Rks & 3;
				dphaseARTable[AR][Rks] =
					EnvPhaseIndex(6 * (RL + 4)) >> (15 - RM);
				break;
			}
			}
		}
	}
}

// Rate Table for Decay
static void makeDphaseDRTable()
{
	for (int DR = 0; DR < 16; ++DR) {
		for (int Rks = 0; Rks < 16; ++Rks) {
			switch(DR) {
			case 0:
				dphaseDRTable[DR][Rks] = EnvPhaseIndex(0);
				break;
			default:
				int RM = std::min(DR + (Rks >> 2), 15);
				int RL = Rks & 3;
				dphaseDRTable[DR][Rks] =
					EnvPhaseIndex(RL + 4) >> (16 - RM);
				break;
			}
		}
	}
}

static void makeRksTable()
{
	for (int fnum8 = 0; fnum8 < 2; ++fnum8) {
		for (int block = 0; block < 8; ++block) {
			for (int KR = 0; KR < 2; ++KR) {
				rksTable[fnum8][block][KR] = (KR != 0)
					? (block << 1) + fnum8
					:  block >> 1;
			}
		}
	}
}

//************************************************************//
//                                                            //
//                      Patch                                 //
//                                                            //
//************************************************************//

Patch::Patch()
	: AM(false), PM(false), EG(false)
	, KR(0), ML(0), KL(0), TL(0)
	, WF(0), AR(0), DR(0), SL(0), RR(0)
{
	setFeedbackShift(0);
}

void Patch::initModulator(const byte* data)
{
	AM = (data[0] >> 7) & 1;
	PM = (data[0] >> 6) & 1;
	EG = (data[0] >> 5) & 1;
	KR = (data[0] >> 4) & 1;
	ML = (data[0] >> 0) & 15;
	KL = (data[2] >> 6) & 3;
	TL = (data[2] >> 0) & 63;
	WF = (data[3] >> 3) & 1;
	AR = (data[4] >> 4) & 15;
	DR = (data[4] >> 0) & 15;
	SL = (data[6] >> 4) & 15;
	RR = (data[6] >> 0) & 15;
	setFeedbackShift((data[3] >> 0) & 7);
}

void Patch::initCarrier(const byte* data)
{
	AM = (data[1] >> 7) & 1;
	PM = (data[1] >> 6) & 1;
	EG = (data[1] >> 5) & 1;
	KR = (data[1] >> 4) & 1;
	ML = (data[1] >> 0) & 15;
	KL = (data[3] >> 6) & 3;
	TL = 0;
	WF = (data[3] >> 4) & 1;
	AR = (data[5] >> 4) & 15;
	DR = (data[5] >> 0) & 15;
	SL = (data[7] >> 4) & 15;
	RR = (data[7] >> 0) & 15;
	setFeedbackShift(0);
}

//************************************************************//
//                                                            //
//                      Slot                                  //
//                                                            //
//************************************************************//
void Slot::reset(bool type_)
{
	type = type_;
	sintbl = waveform[0];
	cphase = 0;
	dphase = 0;
	output = 0;
	feedback = 0;
	setEnvelopeState(FINISH);
	eg_phase = EG_DP_MAX;
	eg_dphase = EnvPhaseIndex(0);
	rks = 0;
	tll = 0;
	sustain = false;
	fnum = 0;
	block = 0;
	volume = 0;
	egout = 0;
	slot_on_flag = false;
}

void Slot::updatePG()
{
	dphase = dphaseTable[fnum][block][patch.ML];
}

void Slot::updateTLL()
{
	tll = tllTable[fnum >> 5][block][type ? volume : patch.TL][patch.KL];
}

void Slot::updateRKS()
{
	rks = rksTable[fnum >> 8][block][patch.KR];
}

void Slot::updateWF()
{
	sintbl = waveform[patch.WF];
}

void Slot::updateEG()
{
	switch (state) {
	case ATTACK:
		eg_dphase = dphaseARTable[patch.AR][rks];
		break;
	case DECAY:
		eg_dphase = dphaseDRTable[patch.DR][rks];
		break;
	case SUSTAIN:
		eg_dphase = dphaseDRTable[patch.RR][rks];
		break;
	case RELEASE:
		if (sustain) {
			eg_dphase = dphaseDRTable[5][rks];
		} else if (patch.EG) {
			eg_dphase = dphaseDRTable[patch.RR][rks];
		} else {
			eg_dphase = dphaseDRTable[7][rks];
		}
		break;
	case SETTLE:
		eg_dphase = dphaseDRTable[15][0];
		break;
	case SUSHOLD:
	case FINISH:
		eg_dphase = EnvPhaseIndex(0);
		break;
	}
}

void Slot::updateAll()
{
	updatePG();
	updateTLL();
	updateRKS();
	updateWF();
	updateEG(); // EG should be updated last
}

void Slot::setEnvelopeState(EnvelopeState state_)
{
	state = state_;
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
	updateEG();
}

// Slot key on, without resetting the phase
void Slot::slotOn2()
{
	setEnvelopeState(ATTACK);
	eg_phase = EnvPhaseIndex(0);
	updateEG();
}

// Slot key off
void Slot::slotOff()
{
	if (state == ATTACK) {
		eg_phase = EnvPhaseIndex(AR_ADJUST_TABLE[eg_phase.toInt()]);
	}
	setEnvelopeState(RELEASE);
	updateEG();
}


// Change a rhythm voice
void Slot::setPatch(Patch& patch)
{
	this->patch = patch; // copy data
}

void Slot::setVolume(int newVolume)
{
	volume = newVolume;
}



//***********************************************************//
//                                                           //
//               Channel                                     //
//                                                           //
//***********************************************************//

void Channel::init(Global& global)
{
	this->global = &global;
	reset();
}

void Channel::reset()
{
	mod.reset(false);
	car.reset(true);
	setPatch(0);
}

// Change a voice
void Channel::setPatch(int num)
{
	patch_number = num;
	mod.setPatch(global->getPatch(num, false));
	car.setPatch(global->getPatch(num, true));
}

// Set sustain parameter
void Channel::setSustain(bool sustain)
{
	car.sustain = sustain;
	if (mod.type) {
		mod.sustain = sustain;
	}
}

// Volume : 6bit ( Volume register << 2 )
void Channel::setVol(int volume)
{
	car.volume = volume;
}

// Set F-Number (fnum : 9bit)
void Channel::setFnumber(int fnum)
{
	car.fnum = fnum;
	mod.fnum = fnum;
}

// Set Block data (block : 3bit)
void Channel::setBlock(int block)
{
	car.block = block;
	mod.block = block;
}

// Channel key on
void Channel::keyOn()
{
	if (!mod.slot_on_flag) mod.slotOn();
	if (!car.slot_on_flag) car.slotOn();
}

// Channel key off
void Channel::keyOff()
{
	// Note: no mod.slotOff() in original code!!!
	if (car.slot_on_flag) car.slotOff();
}


//***********************************************************//
//                                                           //
//               Global                                      //
//                                                           //
//***********************************************************//

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

Global::Global(MSXMotherBoard& motherBoard, const std::string& name,
               const XMLElement& config, const EmuTime& time)
	: YM2413Core(motherBoard, name)
{
	for (int i = 0; i < 16 + 3; ++i) {
		patches[i][0].initModulator(inst_data[i]);
		patches[i][1].initCarrier(inst_data[i]);
	}
	for (int i = 0; i < 9; ++i) {
		channels[i].init(*this);
	}
	makePmTable();
	makeAmTable();
	makeDB2LinTable();
	makeAdjustTable();
	makeTllTable();
	makeRksTable();
	makeSinTable();
	makeDphaseTable();
	makeDphaseARTable();
	makeDphaseDRTable();

	reset(time);
	registerSound(config);
}

Global::~Global()
{
	unregisterSound();
}

// Reset whole of OPLL except patch datas
void Global::reset(const EmuTime& time)
{
	// update the output buffer before changing the registers
	updateStream(time);

	pm_phase = 0;
	am_phase = 0;
	noise_seed = 0xFFFF;
	idleSamples = 0;

	for(int i = 0; i < 9; i++) {
		channels[i].reset();
	}
	for (int i = 0; i < 0x40; i++) {
		writeReg(i, 0, time);
	}
}

// Drum key on
void Global::keyOn_BD()
{
	channels[6].keyOn();
}
void Global::keyOn_HH()
{
	if (!channels[7].mod.slot_on_flag) channels[7].mod.slotOn2();
}
void Global::keyOn_SD()
{
	if (!channels[7].car.slot_on_flag) channels[7].car.slotOn ();
}
void Global::keyOn_TOM()
{
	if (!channels[8].mod.slot_on_flag) channels[8].mod.slotOn ();
}
void Global::keyOn_CYM()
{
	if (!channels[8].car.slot_on_flag) channels[8].car.slotOn2();
}

// Drum key off
void Global::keyOff_BD()
{
	channels[6].keyOff();
}
void Global::keyOff_HH()
{
	if (channels[7].mod.slot_on_flag) channels[7].mod.slotOff();
}
void Global::keyOff_SD()
{
	if (channels[7].car.slot_on_flag) channels[7].car.slotOff();
}
void Global::keyOff_TOM()
{
	if (channels[8].mod.slot_on_flag) channels[8].mod.slotOff();
}
void Global::keyOff_CYM()
{
	if (channels[8].car.slot_on_flag) channels[8].car.slotOff();
}

void Global::update_rhythm_mode()
{
	Channel& ch6 = channels[6];
	if (ch6.patch_number & 0x10) {
		if (!(ch6.car.slot_on_flag || isRhythm())) {
			ch6.mod.setEnvelopeState(FINISH);
			ch6.car.setEnvelopeState(FINISH);
			ch6.setPatch(reg[0x36] >> 4);
		}
	} else if (isRhythm()) {
		ch6.mod.setEnvelopeState(FINISH);
		ch6.car.setEnvelopeState(FINISH);
		ch6.setPatch(16);
	}

	Channel& ch7 = channels[7];
	if (ch7.patch_number & 0x10) {
		if (!((ch7.mod.slot_on_flag && ch7.car.slot_on_flag) ||
		      isRhythm())) {
			ch7.mod.type = false;
			ch7.mod.setEnvelopeState(FINISH);
			ch7.car.setEnvelopeState(FINISH);
			ch7.setPatch(reg[0x37] >> 4);
		}
	} else if (isRhythm()) {
		ch7.mod.type = true;
		ch7.mod.setEnvelopeState(FINISH);
		ch7.car.setEnvelopeState(FINISH);
		ch7.setPatch(17);
	}

	Channel& ch8 = channels[8];
	if (ch8.patch_number & 0x10) {
		if (!((ch8.mod.slot_on_flag && ch8.car.slot_on_flag) ||
		      isRhythm())) {
			ch8.mod.type = false;
			ch8.mod.setEnvelopeState(FINISH);
			ch8.car.setEnvelopeState(FINISH);
			ch8.setPatch(reg[0x38] >> 4);
		}
	} else if (isRhythm()) {
		ch8.mod.type = true;
		ch8.mod.setEnvelopeState(FINISH);
		ch8.car.setEnvelopeState(FINISH);
		ch8.setPatch(18);
	}
}

void Global::update_key_status()
{
	for (int i = 0; i < 9; ++i) {
		bool slot_on = reg[0x20 + i] & 0x10;
		Channel& ch = channels[i];
		ch.mod.slot_on_flag = slot_on;
		ch.car.slot_on_flag = slot_on;
	}
	if (isRhythm()) {
		Channel& ch6 = channels[6];
		ch6.mod.slot_on_flag |= reg[0x0e] & 0x10; // BD1
		ch6.car.slot_on_flag |= reg[0x0e] & 0x10; // BD2
		Channel& ch7 = channels[7];
		ch7.mod.slot_on_flag |= reg[0x0e] & 0x01; // HH
		ch7.car.slot_on_flag |= reg[0x0e] & 0x08; // SD
		Channel& ch8 = channels[8];
		ch8.mod.slot_on_flag |= reg[0x0e] & 0x04; // TOM
		ch8.car.slot_on_flag |= reg[0x0e] & 0x02; // SYM
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
void Slot::calc_phase(PhaseModulation lfo_pm)
{
	if (patch.PM) {
		cphase += (lfo_pm * dphase).toInt();
	} else {
		cphase += dphase;
	}
}

#define S2E(x) EnvPhaseIndex(int(x / EG_STEP))
static const EnvPhaseIndex SL[16] = {
	S2E( 0.0), S2E( 3.0), S2E( 6.0), S2E( 9.0),
	S2E(12.0), S2E(15.0), S2E(18.0), S2E(21.0),
	S2E(24.0), S2E(27.0), S2E(30.0), S2E(33.0),
	S2E(36.0), S2E(39.0), S2E(42.0), S2E(48.0)
};
#undef S2E

// EG
void Slot::calc_envelope(int lfo_am)
{
	unsigned out;
	switch (state) {
	case ATTACK:
		out = AR_ADJUST_TABLE[eg_phase.toInt()];
		eg_phase += eg_dphase;
		if ((eg_phase >= EG_DP_MAX) || (patch.AR == 15)) {
			out = 0;
			eg_phase = EnvPhaseIndex(0);
			setEnvelopeState(DECAY);
			updateEG();
		}
		break;
	case DECAY:
		out = eg_phase.toInt();
		eg_phase += eg_dphase;
		if (eg_phase >= SL[patch.SL]) {
			eg_phase = SL[patch.SL];
			setEnvelopeState(patch.EG ? SUSHOLD : SUSTAIN);
			updateEG();
		}
		break;
	case SUSHOLD:
		out = eg_phase.toInt();
		if (patch.EG == 0) {
			setEnvelopeState(SUSTAIN);
			updateEG();
		}
		break;
	case SUSTAIN:
	case RELEASE:
		out = eg_phase.toInt();
		eg_phase += eg_dphase;
		if (out >= (1 << EG_BITS)) {
			setEnvelopeState(FINISH);
			out = (1 << EG_BITS) - 1;
		}
		break;
	case SETTLE:
		out = eg_phase.toInt();
		eg_phase += eg_dphase;
		if (out >= (1 << EG_BITS)) {
			setEnvelopeState(ATTACK);
			out = (1 << EG_BITS) - 1;
			updateEG();
		}
		break;
	case FINISH:
	default:
		// always results in 'egout = DB_MUTE - 1', but
		// writing it like this is faster for some reason
		out = (1 << EG_BITS) - 1;
	}
	out = EG2DB(out + tll);
	if (patch.AM) {
		out += lfo_am;
	}
	egout = std::min<unsigned>(out, DB_MUTE - 1) | 3;
}

// CARRIER
int Slot::calc_slot_car(int fm)
{
	int phase = (cphase >> DP_BASE_BITS) + wave2_8pi(fm);
	int newOutput = dB2LinTab[sintbl[phase & PG_MASK] + egout];
	output = (output + newOutput) >> 1;
	return output;
}

// MODULATOR
int Slot::calc_slot_mod()
{
	int phase = cphase >> DP_BASE_BITS;
	if (patch.FB) {
		phase += wave2_8pi(feedback) >> patch.FB;
	}
	int newOutput = dB2LinTab[sintbl[phase & PG_MASK] + egout];
	feedback = (output + newOutput) >> 1;
	output = newOutput;
	return feedback;
}

// TOM
int Slot::calc_slot_tom()
{
	int phase = cphase >> DP_BASE_BITS;
	return dB2LinTab[sintbl[phase & PG_MASK] + egout];
}

// SNARE
int Slot::calc_slot_snare(bool noise)
{
	if (BIT(cphase, 7 + DP_BASE_BITS)) {
		return dB2LinTab[(noise ? DB_POS(0.0) : DB_POS(15.0)) + egout];
	} else {
		return dB2LinTab[(noise ? DB_NEG(0.0) : DB_NEG(15.0)) + egout];
	}
}

// TOP-CYM
int Slot::calc_slot_cym(unsigned cphase_hh)
{
	unsigned dbout
	    = (((BIT(cphase_hh, PG_BITS - 8 + DP_BASE_BITS) ^
	         BIT(cphase_hh, PG_BITS - 1 + DP_BASE_BITS)) |
	        BIT(cphase_hh, PG_BITS - 7 + DP_BASE_BITS)) ^
	       ( BIT(cphase, PG_BITS - 7 + DP_BASE_BITS) &
	        !BIT(cphase, PG_BITS - 5 + DP_BASE_BITS)))
	    ? DB_NEG(3.0)
	    : DB_POS(3.0);
	return dB2LinTab[dbout + egout];
}

// HI-HAT
int Slot::calc_slot_hat(unsigned cphase_cym, bool noise)
{
	unsigned dbout;
	if (((BIT(cphase, PG_BITS - 8 + DP_BASE_BITS) ^
	      BIT(cphase, PG_BITS - 1 + DP_BASE_BITS)) |
	     BIT(cphase, PG_BITS - 7 + DP_BASE_BITS)) ^
	    ( BIT(cphase_cym, PG_BITS - 7 + DP_BASE_BITS) &
	     !BIT(cphase_cym, PG_BITS - 5 + DP_BASE_BITS))) {
		dbout = noise ? DB_NEG(12.0) : DB_NEG(24.0);
	} else {
		dbout = noise ? DB_POS(12.0) : DB_POS(24.0);
	}
	return dB2LinTab[dbout + egout];
}

static inline int adjust(int x)
{
	return x << (15 - DB2LIN_AMP_BITS);
}

void Global::generateChannels(int** bufs, unsigned num)
{
	// TODO make channelActiveBits a member and
	//      keep it up-to-date all the time

	// bits 0-8  -> ch[0-8].car
	// bits 9-17 -> ch[0-8].mod (only ch7 and ch8 are used)
	unsigned channelActiveBits = 0;

	for (int ch = 0; ch < 9; ++ch) {
		if (channels[ch].car.isActive()) {
			channelActiveBits |= 1 << ch;
		} else {
			bufs[ch] = 0;
		}
	}
	if (isRhythm()) {
		if (channels[7].mod.isActive()) {
			channelActiveBits |= 1 << (7 + 9);
		} else {
			bufs[9] = 0;
		}
		if (channels[8].mod.isActive()) {
			channelActiveBits |= 1 << (8 + 9);
		} else {
			bufs[10] = 0;
		}
	} else {
		bufs[ 9] = 0;
		bufs[10] = 0;
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

	int m = isRhythm() ? 6 : 9;
	for (unsigned sample = 0; sample < num; ++sample) {
		// update Noise unit
		if (noise_seed & 1) noise_seed ^= 0x8003020;
		noise_seed >>= 1;

		// update AM, PM unit
		pm_phase = (pm_phase + PM_DPHASE) & (PM_DP_WIDTH - 1);
		am_phase = (am_phase + AM_DPHASE) & (AM_DP_WIDTH - 1);
		int lfo_am =
			amtable[HIGHBITS(am_phase, AM_DP_BITS - AM_PG_BITS)];
		PhaseModulation lfo_pm =
			pmtable[HIGHBITS(pm_phase, PM_DP_BITS - PM_PG_BITS)];

		// TODO: Is there a point in updating the carrier when envelope is
		//       in FINISH state? And what about the modulator?
		/*for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			ch.mod.calc_phase(lfo_pm);
			ch.mod.calc_envelope(lfo_am);
			ch.car.calc_phase(lfo_pm);
			ch.car.calc_envelope(lfo_am);
		}*/

		for (int i = 0; i < m; ++i) {
			Channel& ch = channels[i];
			ch.mod.calc_phase(lfo_pm);
			ch.mod.calc_envelope(lfo_am);
			ch.car.calc_phase(lfo_pm);
			ch.car.calc_envelope(lfo_am);
			if (channelActiveBits & (1 << i)) {
				bufs[i][sample] =
					adjust(ch.car.calc_slot_car(ch.mod.calc_slot_mod()));
			}
		}
		if (isRhythm()) {
			Channel& ch6 = channels[6];
			ch6.mod.calc_phase(lfo_pm);
			ch6.mod.calc_envelope(lfo_am);
			ch6.car.calc_phase(lfo_pm);
			ch6.car.calc_envelope(lfo_am);
			if (channelActiveBits & (1 << 6)) {
				bufs[ 6][sample] =
					adjust(2 * ch6.car.calc_slot_car(ch6.mod.calc_slot_mod()));
			}

			Channel& ch7 = channels[7];
			ch7.car.calc_phase(lfo_pm);
			ch7.car.calc_envelope(lfo_am);
			if (channelActiveBits & (1 << 7)) {
				bufs[ 7][sample] =
					adjust(-2 * ch7.car.calc_slot_snare(noise_seed & 1));
			}

			Channel& ch8 = channels[8];
			ch8.car.calc_phase(lfo_pm);
			ch8.car.calc_envelope(lfo_am);
			ch7.mod.calc_phase(lfo_pm);
			ch7.mod.calc_envelope(lfo_am);
			if (channelActiveBits & (1 << 8)) {
				bufs[ 8][sample] =
					adjust(-2 * ch8.car.calc_slot_cym(ch7.mod.cphase));
			}
			if (channelActiveBits & (1 << (7 + 9))) {
				bufs[ 9][sample] =
					adjust(2 * ch7.mod.calc_slot_hat(ch8.car.cphase,
									 noise_seed & 1));
			}

			ch8.mod.calc_phase(lfo_pm);
			ch8.mod.calc_envelope(lfo_am);
			if (channelActiveBits & (1 << (8 + 9))) {
				bufs[10][sample] =
					adjust(2 * ch8.mod.calc_slot_tom());
			}
		}
	}
}

void Global::writeReg(byte regis, byte data, const EmuTime& time)
{
	//PRT_DEBUG("YM2413: write reg "<<(int)regis<<" "<<(int)data);

	// update the output buffer before changing the register
	updateStream(time);

	assert (regis < 0x40);
	reg[regis] = data;

	switch (regis) {
	case 0x00:
		patches[0][0].AM = (data >> 7) & 1;
		patches[0][0].PM = (data >> 6) & 1;
		patches[0][0].EG = (data >> 5) & 1;
		patches[0][0].KR = (data >> 4) & 1;
		patches[0][0].ML = (data >> 0) & 15;
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.mod.updatePG();
				ch.mod.updateRKS();
				ch.mod.updateEG();
			}
		}
		break;
	case 0x01:
		patches[0][1].AM = (data >> 7) & 1;
		patches[0][1].PM = (data >> 6) & 1;
		patches[0][1].EG = (data >> 5) & 1;
		patches[0][1].KR = (data >> 4) & 1;
		patches[0][1].ML = (data >> 0) & 15;
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if(ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.car.updatePG();
				ch.car.updateRKS();
				ch.car.updateEG();
			}
		}
		break;
	case 0x02:
		patches[0][0].KL = (data >> 6) & 3;
		patches[0][0].TL = (data >> 0) & 63;
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.mod.updateTLL();
			}
		}
		break;
	case 0x03:
		patches[0][1].KL = (data >> 6) & 3;
		patches[0][1].WF = (data >> 4) & 1;
		patches[0][0].WF = (data >> 3) & 1;
		patches[0][0].setFeedbackShift((data >> 0) & 7);
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.mod.updateWF();
				ch.car.updateWF();
			}
		}
		break;
	case 0x04:
		patches[0][0].AR = (data >> 4) & 15;
		patches[0][0].DR = (data >> 0) & 15;
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.mod.updateEG();
			}
		}
		break;
	case 0x05:
		patches[0][1].AR = (data >> 4) & 15;
		patches[0][1].DR = (data >> 0) & 15;
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.car.updateEG();
			}
		}
		break;
	case 0x06:
		patches[0][0].SL = (data >> 4) & 15;
		patches[0][0].RR = (data >> 0) & 15;
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.mod.updateEG();
			}
		}
		break;
	case 0x07:
		patches[0][1].SL = (data >> 4) & 15;
		patches[0][1].RR = (data >> 0) & 15;
		for (int i = 0; i < 9; i++) {
			Channel& ch = channels[i];
			if (ch.patch_number == 0) {
				ch.setPatch(0); // TODO optimize
				ch.car.updateEG();
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
		update_key_status();

		Channel& ch6 = channels[6];
		ch6.mod.updateAll();
		ch6.car.updateAll();
		Channel& ch7 = channels[7];
		ch7.mod.updateAll();
		ch7.car.updateAll();
		Channel& ch8 = channels[8];
		ch8.mod.updateAll();
		ch8.car.updateAll();
		break;
	}
	case 0x10:  case 0x11:  case 0x12:  case 0x13:
	case 0x14:  case 0x15:  case 0x16:  case 0x17:
	case 0x18:
	{
		int cha = regis & 0x0F;
		Channel& ch = channels[cha];
		ch.setFnumber(data + ((reg[0x20 + cha] & 1) << 8));
		ch.mod.updateAll();
		ch.car.updateAll();
		break;
	}
	case 0x20:  case 0x21:  case 0x22:  case 0x23:
	case 0x24:  case 0x25:  case 0x26:  case 0x27:
	case 0x28:
	{
		int cha = regis & 0x0F;
		Channel& ch = channels[cha];
		int fNum = ((data & 1) << 8) + reg[0x10 + cha];
		int block = (data >> 1) & 7;
		ch.setFnumber(fNum);
		ch.setBlock(block);
		ch.setSustain((data >> 5) & 1);
		if (data & 0x10) {
			ch.keyOn();
		} else {
			ch.keyOff();
		}
		ch.mod.updateAll();
		ch.car.updateAll();
		update_key_status();
		update_rhythm_mode();
		break;
	}
	case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
	case 0x35: case 0x36: case 0x37: case 0x38:
	{
		int cha = regis & 0x0F;
		Channel& ch = channels[cha];
		int j = (data >> 4) & 15;
		int v = data & 15;
		if (isRhythm() && (cha >= 6)) {
			if (cha > 6) {
				// channel 7 or 8 in ryhthm mode
				channels[cha].mod.setVolume(j << 2);
			}
		} else {
			ch.setPatch(j);
		}
		ch.setVol(v << 2);
		ch.mod.updateAll();
		ch.car.updateAll();
		break;
	}
	default:
		break;
	}
}

} // namespace YM2413Okazaki


// class YM2413

YM2413::YM2413(MSXMotherBoard& motherBoard, const std::string& name,
               const XMLElement& config, const EmuTime& time)
	: global(new YM2413Okazaki::Global(motherBoard, name, config, time))
{
}

YM2413::~YM2413()
{
}

void YM2413::reset(const EmuTime& time)
{
	global->reset(time);
}

void YM2413::writeReg(byte regis, byte data, const EmuTime& time)
{
	global->writeReg(regis, data, time);
}

} // namespace openmsx
