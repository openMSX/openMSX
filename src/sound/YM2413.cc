// $Id$

/*
 * Based on:
 *    emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001
 * heavily rewritten to fit openMSX structure
 */

#include "YM2413.hh"
#include "YM2413Core.hh"
#include "FixedPoint.hh"
#include "serialize.hh"
#include "inline.hh"
#include <cmath>
#include <cassert>
#include <algorithm>

namespace openmsx {

namespace YM2413Okazaki {

typedef FixedPoint<8> PhaseModulation;
typedef FixedPoint<15> EnvPhaseIndex;
static const EnvPhaseIndex EG_DP_MAX = EnvPhaseIndex(1 << 7);
static const EnvPhaseIndex EG_DP_INF = EnvPhaseIndex(1 << 8); // as long as it's bigger

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

	/** Sets the amount of feedback [0..7].
	 */
	void setFeedbackShift(byte value) {
		FB = value ? 8 - value : 0;
	}

	/** Sets the Key Scale of Rate (0 or 1).
	 */
	void setKeyScaleRate(bool value) {
		KR = value ? 8 : 10;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	bool AM, PM, EG;
	byte KR; // 0,1  transformed to  10,8
	byte ML; // 0-15
	byte KL; // 0-3
	byte TL; // 0-63
	byte FB; // 0,1-7  transformed to  0,7-1
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
	inline void setVolume(unsigned volume);

	template <bool HAS_PM> inline unsigned calc_phase(PhaseModulation lfo_pm);
	template <bool HAS_AM> inline unsigned calc_envelope(int lfo_am);
	void calc_envelope_outline(unsigned& out);
	template <bool HAS_PM, bool HAS_AM>
	inline int calc_slot_car(PhaseModulation lfo_pm, int lfo_am, int fm);
	template <bool HAS_PM, bool HAS_AM, bool HAS_FB>
	inline int calc_slot_mod(PhaseModulation lfo_pm, int lfo_am);

	inline int calc_slot_tom();
	inline int calc_slot_snare(bool noise);
	inline int calc_slot_cym(unsigned phase7, unsigned phase8);
	inline int calc_slot_hat(unsigned phase7, unsigned phase8, bool noise);
	inline void updatePG(unsigned freq);
	inline void updateTLL(unsigned freq);
	inline void updateRKS(unsigned freq);
	inline void updateWF();
	inline void updateEG();
	inline void updateAll(unsigned freq);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	Patch patch;
	unsigned* sintbl;	// Wavetable (for PG)

	// OUTPUT
	int feedback;
	int output;		// Output value of slot

	// for Phase Generator (PG)
	unsigned cphase;	// Phase counter
	unsigned dphase;	// Phase increment

	// for Envelope Generator (EG)
	unsigned volume;	// Current volume
	unsigned tll;		// Total Level + Key scale level
	EnvPhaseIndex* dphaseARTableRks;
	EnvPhaseIndex* dphaseDRTableRks;
	EnvelopeState state;	// Current state
	EnvPhaseIndex eg_phase;	// Phase
	EnvPhaseIndex eg_dphase;// Phase increment amount
	EnvPhaseIndex eg_phase_max;
	bool sustain;		// Sustain
	bool type;		// 0 : modulator 1 : carrier
	bool slot_on_flag;
};

class Channel {
public:
	void reset(Global& global);
	inline void setPatch(unsigned num, Global& global);
	inline void setSustain(bool sustain);
	inline void setVol(unsigned volume);
	inline void setFreq(unsigned freq);
	inline void keyOn();
	inline void keyOff();

	Slot mod, car;
	unsigned patch_number;
	unsigned patchFlags;

	unsigned freq; // combined fnum and block

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
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

	virtual int getAmplificationFactor() const;
	virtual void generateChannels(int** bufs, unsigned num);

	template <unsigned FLAGS>
	inline void calcChannel(Channel& ch, int* buf, unsigned num);

	Patch& getPatch(unsigned instrument, bool carrier) {
		return patches[instrument][carrier];
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

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
	unsigned noise_seed;

	// Number of samples the output was completely silent
	unsigned idleSamples;
};


// Dynamic range (Accuracy of sin table)
static const int DB_BITS = 8;
static const int DB_MUTE = 1 << DB_BITS;

static const double DB_STEP = 48.0 / DB_MUTE;
static const double EG_STEP = 0.375;
static const double TL_STEP = 0.75;

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
static const int PM_DP_MASK = PM_DP_WIDTH - 1;
static const int AM_PG_BITS = 8;
static const int AM_PG_WIDTH = 1 << AM_PG_BITS;
static const int AM_DP_BITS = 16;
static const int AM_DP_WIDTH = 1 << AM_DP_BITS;
static const int AM_DP_MASK = AM_DP_WIDTH - 1;

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

// Phase incr table for Attack (note: indices swapped compared to original code)
static EnvPhaseIndex dphaseARTable[16][16];
// Phase incr table for Decay and Release (note: indices swapped)
static EnvPhaseIndex dphaseDRTable[16][16];

// KSL + TL Table
static unsigned tllTable[16 * 8][1 << TL_BITS][4];

// Phase incr table for PG
static unsigned dphaseTable[512 * 8][16];


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

static inline bool BIT(unsigned s, unsigned b)
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

	for (unsigned freq = 0; freq < 8 * 512; ++freq) {
		unsigned fnum = freq & 511;
		unsigned block = freq / 512;
		for (unsigned ML = 0; ML < 16; ++ML) {
			dphaseTable[freq][ML] =
				((fnum * mltable[ML]) << block) >> (20 - DP_BITS);
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

	for (unsigned freq = 0; freq < 16 * 8; ++freq) {
		unsigned fnum = freq & 15;
		unsigned block = freq / 16;
		for (unsigned TL = 0; TL < 64; ++TL) {
			tllTable[freq][TL][0] = TL2EG(TL);
			for (unsigned KL = 1; KL < 4; ++KL) {
				int tmp = int(
					kltable[fnum] - (3.000 * 2) * (7 - block));
				tllTable[freq][TL][KL] =
					(tmp <= 0) ?
					TL2EG(TL) :
					unsigned((tmp >> (3 - KL)) / EG_STEP) + TL2EG(TL);
			}
		}
	}
}

// Rate Table for Attack
static void makeDphaseARTable()
{
	for (unsigned Rks = 0; Rks < 16; ++Rks) {
		dphaseARTable[Rks][0] = EnvPhaseIndex(0);
		for (unsigned AR = 1; AR < 15; ++AR) {
			unsigned RM = std::min(AR + (Rks >> 2), 15u);
			unsigned RL = Rks & 3;
			dphaseARTable[Rks][AR] =
				EnvPhaseIndex(12 * (RL + 4)) >> (16 - RM);
		}
		dphaseARTable[Rks][15] = EnvPhaseIndex(0); // EG_DP_MAX
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

//************************************************************//
//                                                            //
//                      Patch                                 //
//                                                            //
//************************************************************//

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
	dphaseARTableRks = dphaseARTable[0];
	dphaseDRTableRks = dphaseDRTable[0];
	tll = 0;
	sustain = false;
	volume = 0;
	slot_on_flag = false;
}

void Slot::updatePG(unsigned freq)
{
	dphase = dphaseTable[freq][patch.ML];
}

void Slot::updateTLL(unsigned freq)
{
	tll = tllTable[freq >> 5][type ? volume : patch.TL][patch.KL];
}

void Slot::updateRKS(unsigned freq)
{
	unsigned rks = freq >> patch.KR;
	assert(rks < 16);
	dphaseARTableRks = dphaseARTable[rks];
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
		eg_dphase = dphaseARTableRks[patch.AR];
		break;
	case DECAY:
		eg_dphase = dphaseDRTableRks[patch.DR];
		break;
	case SUSTAIN:
		eg_dphase = dphaseDRTableRks[patch.RR];
		break;
	case RELEASE: {
		unsigned idx = sustain ? 5
		                       : (patch.EG ? patch.AR
		                                   : 7);
		eg_dphase = dphaseDRTableRks[idx];
		break;
	}
	case SETTLE:
		eg_dphase = dphaseDRTable[0][15];
		break;
	case SUSHOLD:
	case FINISH:
		eg_dphase = EnvPhaseIndex(0);
		break;
	}
}

void Slot::updateAll(unsigned freq)
{
	updatePG(freq);
	updateTLL(freq);
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
	case SUSTAIN:
	case RELEASE:
		eg_phase_max = EG_DP_MAX;
		break;
	case SETTLE:
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



//***********************************************************//
//                                                           //
//               Channel                                     //
//                                                           //
//***********************************************************//

void Channel::reset(Global& global)
{
	freq = 0;
	mod.reset(false);
	car.reset(true);
	setPatch(0, global);
}

// Change a voice
void Channel::setPatch(unsigned num, Global& global)
{
	patch_number = num;
	mod.setPatch(global.getPatch(num, false));
	car.setPatch(global.getPatch(num, true));
	patchFlags = ( car.patch.AM       << 0) |
	             ( car.patch.PM       << 1) |
	             ( mod.patch.AM       << 2) |
	             ( mod.patch.PM       << 3) |
	             ((mod.patch.FB != 0) << 4);
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
	for (unsigned i = 0; i < 16 + 3; ++i) {
		patches[i][0].initModulator(inst_data[i]);
		patches[i][1].initCarrier(inst_data[i]);
	}
	makePmTable();
	makeAmTable();
	makeDB2LinTable();
	makeAdjustTable();
	makeTllTable();
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

	for (unsigned i = 0; i < 9; i++) {
		channels[i].reset(*this);
	}
	for (unsigned i = 0; i < 0x40; i++) {
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
			ch6.setPatch(reg[0x36] >> 4, *this);
		}
	} else if (isRhythm()) {
		ch6.mod.setEnvelopeState(FINISH);
		ch6.car.setEnvelopeState(FINISH);
		ch6.setPatch(16, *this);
	}

	Channel& ch7 = channels[7];
	if (ch7.patch_number & 0x10) {
		if (!((ch7.mod.slot_on_flag && ch7.car.slot_on_flag) ||
		      isRhythm())) {
			ch7.mod.type = false;
			ch7.mod.setEnvelopeState(FINISH);
			ch7.car.setEnvelopeState(FINISH);
			ch7.setPatch(reg[0x37] >> 4, *this);
		}
	} else if (isRhythm()) {
		ch7.mod.type = true;
		ch7.mod.setEnvelopeState(FINISH);
		ch7.car.setEnvelopeState(FINISH);
		ch7.setPatch(17, *this);
	}

	Channel& ch8 = channels[8];
	if (ch8.patch_number & 0x10) {
		if (!((ch8.mod.slot_on_flag && ch8.car.slot_on_flag) ||
		      isRhythm())) {
			ch8.mod.type = false;
			ch8.mod.setEnvelopeState(FINISH);
			ch8.car.setEnvelopeState(FINISH);
			ch8.setPatch(reg[0x38] >> 4, *this);
		}
	} else if (isRhythm()) {
		ch8.mod.type = true;
		ch8.mod.setEnvelopeState(FINISH);
		ch8.car.setEnvelopeState(FINISH);
		ch8.setPatch(18, *this);
	}
}

void Global::update_key_status()
{
	for (unsigned i = 0; i < 9; ++i) {
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
		setEnvelopeState(ATTACK);
		break;
	case SUSHOLD:
	case FINISH:
	default:
		assert(false);
		break;
	}
}
template <bool HAS_AM>
ALWAYS_INLINE unsigned Slot::calc_envelope(int lfo_am)
{
	assert(patch.AM == HAS_AM);
	unsigned out = eg_phase.toInt();
	if (state == ATTACK) {
		out = AR_ADJUST_TABLE[out];
	}
	eg_phase += eg_dphase;
	if (eg_phase >= eg_phase_max) {
		calc_envelope_outline(out);
	}
	out = EG2DB(out + tll);

	if (HAS_AM) {
		out += lfo_am;
	}
	return std::min<unsigned>(out, DB_MUTE - 1) | 3;
}

// CARRIER
template <bool HAS_PM, bool HAS_AM>
ALWAYS_INLINE int Slot::calc_slot_car(PhaseModulation lfo_pm, int lfo_am, int fm)
{
	int phase = calc_phase<HAS_PM>(lfo_pm) + wave2_8pi(fm);
	unsigned egout = calc_envelope<HAS_AM>(lfo_am);
	int newOutput = dB2LinTab[sintbl[phase & PG_MASK] + egout];
	output = (output + newOutput) >> 1;
	return output;
}

// MODULATOR
template <bool HAS_PM, bool HAS_AM, bool HAS_FB>
ALWAYS_INLINE int Slot::calc_slot_mod(PhaseModulation lfo_pm, int lfo_am)
{
	assert((patch.FB != 0) == HAS_FB);
	unsigned phase = calc_phase<HAS_PM>(lfo_pm);
	unsigned egout = calc_envelope<HAS_AM>(lfo_am);
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
	unsigned egout = calc_envelope<false>(0);
	return dB2LinTab[sintbl[phase & PG_MASK] + egout];
}

// SNARE (ch7 car)
ALWAYS_INLINE int Slot::calc_slot_snare(bool noise)
{
	unsigned phase = calc_phase<false>(PhaseModulation());
	unsigned egout = calc_envelope<false>(0);
	return BIT(phase, 7)
		? dB2LinTab[(noise ? DB_POS(0.0) : DB_POS(15.0)) + egout]
		: dB2LinTab[(noise ? DB_NEG(0.0) : DB_NEG(15.0)) + egout];
}

// TOP-CYM (ch8 car)
ALWAYS_INLINE int Slot::calc_slot_cym(unsigned phase7, unsigned phase8)
{
	unsigned egout = calc_envelope<false>(0);
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
	unsigned egout = calc_envelope<false>(0);
	unsigned dbout = (((BIT(phase7, PG_BITS - 8) ^
	                    BIT(phase7, PG_BITS - 1)) |
	                   BIT(phase7, PG_BITS - 7)) ^
	                  ( BIT(phase8, PG_BITS - 7) &
	                   !BIT(phase8, PG_BITS - 5)))
	               ? (noise ? DB_NEG(12.0) : DB_NEG(24.0))
	               : (noise ? DB_POS(12.0) : DB_POS(24.0));
	return dB2LinTab[dbout + egout];
}

int Global::getAmplificationFactor() const
{
	return 1 << (15 - DB2LIN_AMP_BITS);
}

template <unsigned FLAGS>
ALWAYS_INLINE void Global::calcChannel(Channel& ch, int* buf, unsigned num)
{
	const bool HAS_CAR_AM = FLAGS &  1;
	const bool HAS_CAR_PM = FLAGS &  2;
	const bool HAS_MOD_AM = FLAGS &  4;
	const bool HAS_MOD_PM = FLAGS &  8;
	const bool HAS_MOD_FB = FLAGS & 16;

	unsigned tmp_pm_phase = pm_phase;
	unsigned tmp_am_phase = am_phase;
	for (unsigned sample = 0; sample < num; ++sample) {
		PhaseModulation lfo_pm;
		if (HAS_CAR_PM || HAS_MOD_PM) {
			tmp_pm_phase = (tmp_pm_phase + PM_DPHASE) & PM_DP_MASK;
			lfo_pm = pmtable[tmp_pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
		}
		int lfo_am = 0; // avoid warning
		if (HAS_CAR_AM || HAS_MOD_AM) {
			tmp_am_phase = (tmp_am_phase + AM_DPHASE) & AM_DP_MASK;
			lfo_am = amtable[tmp_am_phase >> (AM_DP_BITS - AM_PG_BITS)];
		}
		int fm = ch.mod.calc_slot_mod<HAS_MOD_PM, HAS_MOD_AM, HAS_MOD_FB>(
		                      lfo_pm, lfo_am);
		buf[sample] += ch.car.calc_slot_car<HAS_CAR_PM, HAS_CAR_AM>(
		                      lfo_pm, lfo_am, fm);
	}
}

void Global::generateChannels(int** bufs, unsigned num)
{
	// TODO make channelActiveBits a member and
	//      keep it up-to-date all the time

	// bits 0-8  -> ch[0-8].car
	// bits 9-17 -> ch[0-8].mod (only ch7 and ch8 are used)
	unsigned channelActiveBits = 0;

	for (unsigned ch = 0; ch < 9; ++ch) {
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

	unsigned m = isRhythm() ? 6 : 9;
	for (unsigned i = 0; i < m; ++i) {
		if (channelActiveBits & (1 << i)) {
			// below we choose between 32 specialized versions of
			// calcChannel() this allows to move a lot of
			// conditional code out of the inner-loop
			Channel& ch = channels[i];
			switch (ch.patchFlags) {
			case  0: calcChannel< 0>(ch, bufs[i], num); break;
			case  1: calcChannel< 1>(ch, bufs[i], num); break;
			case  2: calcChannel< 2>(ch, bufs[i], num); break;
			case  3: calcChannel< 3>(ch, bufs[i], num); break;
			case  4: calcChannel< 4>(ch, bufs[i], num); break;
			case  5: calcChannel< 5>(ch, bufs[i], num); break;
			case  6: calcChannel< 6>(ch, bufs[i], num); break;
			case  7: calcChannel< 7>(ch, bufs[i], num); break;
			case  8: calcChannel< 8>(ch, bufs[i], num); break;
			case  9: calcChannel< 9>(ch, bufs[i], num); break;
			case 10: calcChannel<10>(ch, bufs[i], num); break;
			case 11: calcChannel<11>(ch, bufs[i], num); break;
			case 12: calcChannel<12>(ch, bufs[i], num); break;
			case 13: calcChannel<13>(ch, bufs[i], num); break;
			case 14: calcChannel<14>(ch, bufs[i], num); break;
			case 15: calcChannel<15>(ch, bufs[i], num); break;
			case 16: calcChannel<16>(ch, bufs[i], num); break;
			case 17: calcChannel<17>(ch, bufs[i], num); break;
			case 18: calcChannel<18>(ch, bufs[i], num); break;
			case 19: calcChannel<19>(ch, bufs[i], num); break;
			case 20: calcChannel<20>(ch, bufs[i], num); break;
			case 21: calcChannel<21>(ch, bufs[i], num); break;
			case 22: calcChannel<22>(ch, bufs[i], num); break;
			case 23: calcChannel<23>(ch, bufs[i], num); break;
			case 24: calcChannel<24>(ch, bufs[i], num); break;
			case 25: calcChannel<25>(ch, bufs[i], num); break;
			case 26: calcChannel<26>(ch, bufs[i], num); break;
			case 27: calcChannel<27>(ch, bufs[i], num); break;
			case 28: calcChannel<28>(ch, bufs[i], num); break;
			case 29: calcChannel<29>(ch, bufs[i], num); break;
			case 30: calcChannel<30>(ch, bufs[i], num); break;
			case 31: calcChannel<31>(ch, bufs[i], num); break;
			default: assert(false);
			}
		}
	}
	// update AM, PM unit
	pm_phase += num * PM_DPHASE;
	am_phase += num * AM_DPHASE;

	if (isRhythm()) {
		if (channelActiveBits & (1 << 6)) {
			Channel& ch6 = channels[6];
			for (unsigned sample = 0; sample < num; ++sample) {
				bufs[6][sample] += 2 *
				    ch6.car.calc_slot_car<false, false>(
				        PhaseModulation(), 0, ch6.mod.calc_slot_mod<
				                false, false, false>(PhaseModulation(), 0));
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
				bufs[7][sample] +=
					-2 * ch7.car.calc_slot_snare(noise_bit);
			}
		}
		unsigned old_cphase7 = ch7.mod.cphase;
		unsigned old_cphase8 = ch8.car.cphase;
		if (channelActiveBits & (1 << 8)) {
			for (unsigned sample = 0; sample < num; ++sample) {
				unsigned phase7 = ch7.mod.calc_phase<false>(PhaseModulation());
				unsigned phase8 = ch8.car.calc_phase<false>(PhaseModulation());
				bufs[8][sample] +=
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
				bufs[9][sample] +=
					2 * ch7.mod.calc_slot_hat(phase7, phase8, noise_bit);
			}
		}
		if (channelActiveBits & (1 << (8 + 9))) {
			for (unsigned sample = 0; sample < num; ++sample) {
				bufs[10][sample] += 2 * ch8.mod.calc_slot_tom();
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
				ch.mod.updateTLL(ch.freq);
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
		update_key_status();

		Channel& ch6 = channels[6];
		ch6.mod.updateAll(ch6.freq);
		ch6.car.updateAll(ch6.freq);
		Channel& ch7 = channels[7];
		ch7.mod.updateAll(ch7.freq);
		ch7.car.updateAll(ch7.freq);
		Channel& ch8 = channels[8];
		ch8.mod.updateAll(ch8.freq);
		ch8.car.updateAll(ch8.freq);
		break;
	}
	case 0x10:  case 0x11:  case 0x12:  case 0x13:
	case 0x14:  case 0x15:  case 0x16:  case 0x17:
	case 0x18:
	{
		unsigned cha = regis & 0x0F;
		Channel& ch = channels[cha];
		ch.setFreq((reg[0x20 + cha] & 0xF) << 8 | data);
		ch.mod.updateAll(ch.freq);
		ch.car.updateAll(ch.freq);
		break;
	}
	case 0x20:  case 0x21:  case 0x22:  case 0x23:
	case 0x24:  case 0x25:  case 0x26:  case 0x27:
	case 0x28:
	{
		unsigned cha = regis & 0x0F;
		Channel& ch = channels[cha];
		ch.setFreq((data & 0xF) << 8 | reg[0x10 + cha]);
		ch.setSustain((data >> 5) & 1);
		if (data & 0x10) {
			ch.keyOn();
		} else {
			ch.keyOff();
		}
		ch.mod.updateAll(ch.freq);
		ch.car.updateAll(ch.freq);
		update_key_status();
		update_rhythm_mode();
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
		ch.mod.updateAll(ch.freq);
		ch.car.updateAll(ch.freq);
		break;
	}
	default:
		break;
	}
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

template<typename Archive>
void Slot::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("patch", patch);
	ar.serialize("feedback", feedback);
	ar.serialize("output", output);
	ar.serialize("cphase", cphase);
	ar.serialize("volume", volume);
	ar.serialize("state", state);
	ar.serialize("eg_phase", eg_phase);
	ar.serialize("sustain", sustain);
	ar.serialize("type", type);
	ar.serialize("slot_on_flag", slot_on_flag);

	// These are restored by call to updateAll() in Channel::serialize()
	//   eg_dphase, dphaseARTableRks, dphaseDRTableRks, tll, dphase, sintbl
	// and by setEnvelopeState()
	//   eg_phase_max
}

template<typename Archive>
void Channel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mod", mod);
	ar.serialize("car", car);
	ar.serialize("patch_number", patch_number);
	ar.serialize("patchFlags", patchFlags);
	ar.serialize("freq", freq);

	if (ar.isLoader()) {
		mod.updateAll(freq);
		car.updateAll(freq);
		mod.setEnvelopeState(mod.state);
		car.setEnvelopeState(car.state);
	}
}

template<typename Archive>
void Global::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<YM2413Core>(*this);
	// no need to serialize patches[1-19]
	ar.serialize("user_patch_mod", patches[0][0]);
	ar.serialize("user_patch_car", patches[0][1]);
	ar.serialize("channels", channels);
	ar.serialize("pm_phase", pm_phase);
	ar.serialize("am_phase", am_phase);
	ar.serialize("noise_seed", noise_seed);
	// don't serialize idleSamples, is only an optimization
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

template<typename Archive>
void YM2413::serialize(Archive& ar, unsigned version)
{
	global->serialize(ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(YM2413);

} // namespace openmsx
