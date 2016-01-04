/*
 * Based on:
 *    emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001
 * heavily rewritten to fit openMSX structure
 */

#include "YM2413Okazaki.hh"
#include "serialize.hh"
#include "inline.hh"
#include "unreachable.hh"
#include <cstring>
#include <cassert>

namespace openmsx {
namespace YM2413Okazaki {

// This defines the tables:
//  - signed char pmTable[8][8]
//  - int dB2LinTab[DBTABLEN * 2]
//  - unsigned AR_ADJUST_TABLE[1 << EG_BITS]
//  - byte tllTable[4][16 * 8]
//  - unsigned* waveform[2]
//  - int dphaseDRTable[16][16]
//  - byte lfo_am_table[LFO_AM_TAB_ELEMENTS]
//  - unsigned slTable[16]
//  - byte mlTable[16]
#include "YM2413OkazakiTable.ii"


// Extra (derived) constants
static const EnvPhaseIndex EG_DP_MAX = EnvPhaseIndex(1 << 7);
static const EnvPhaseIndex EG_DP_INF = EnvPhaseIndex(1 << 8); // as long as it's bigger


//
// Helper functions
//
static inline int EG2DB(int d)
{
	return d * int(EG_STEP / DB_STEP);
}
static inline unsigned TL2EG(unsigned d)
{
	assert(d < 64); // input is in range [0..63]
	return d * int(TL_STEP / EG_STEP);
}

static inline unsigned DB_POS(float x)
{
	int result = int(x / DB_STEP);
	assert(0 <= result);
	assert(result < DB_MUTE);
	return result;
}
static inline unsigned DB_NEG(float x)
{
	return DBTABLEN + DB_POS(x);
}

static inline bool BIT(unsigned s, unsigned b)
{
	return (s >> b) & 1;
}

//
// Patch
//
Patch::Patch()
	: AMPM(0), EG(false), AR(0), DR(0), RR(0)
{
	setKR(0);
	setML(0);
	setKL(0);
	setTL(0);
	setWF(0);
	setFB(0);
	setSL(0);
}

void Patch::initModulator(const byte* data)
{
	AMPM = (data[0] >> 6) &  3;
	EG   = (data[0] >> 5) &  1;
	setKR ((data[0] >> 4) &  1);
	setML ((data[0] >> 0) & 15);
	setKL ((data[2] >> 6) &  3);
	setTL ((data[2] >> 0) & 63);
	setWF ((data[3] >> 3) &  1);
	setFB ((data[3] >> 0) &  7);
	AR   = (data[4] >> 4) & 15;
	DR   = (data[4] >> 0) & 15;
	setSL ((data[6] >> 4) & 15);
	RR   = (data[6] >> 0) & 15;
}

void Patch::initCarrier(const byte* data)
{
	AMPM = (data[1] >> 6) &  3;
	EG   = (data[1] >> 5) &  1;
	setKR ((data[1] >> 4) &  1);
	setML ((data[1] >> 0) & 15);
	setKL ((data[3] >> 6) &  3);
	setTL (0);
	setWF ((data[3] >> 4) &  1);
	setFB (0);
	AR   = (data[5] >> 4) & 15;
	DR   = (data[5] >> 0) & 15;
	setSL ((data[7] >> 4) & 15);
	RR   = (data[7] >> 0) & 15;
}

void Patch::setKR(byte value)
{
	KR = value ? 8 : 10;
}
void Patch::setML(byte value)
{
	ML = mlTable[value];
}
void Patch::setKL(byte value)
{
	KL = tllTable[value];
}
void Patch::setTL(byte value)
{
	assert(value < 64);
	assert(TL2EG(value) < 256);
	TL = TL2EG(value);
}
void Patch::setWF(byte value)
{
	WF = waveform[value];
}
void Patch::setFB(byte value)
{
	FB = value ? 8 - value : 0;
}
void Patch::setSL(byte value)
{
	SL = slTable[value];
}


//
// Slot
//
void Slot::reset()
{
	cphase = 0;
	for (auto& dp : dphase) dp = 0;
	output = 0;
	feedback = 0;
	setEnvelopeState(FINISH);
	dphaseDRTableRks = dphaseDRTable[0];
	tll = 0;
	sustain = false;
	volume = TL2EG(0);
	slot_on_flag = 0;
}

void Slot::updatePG(unsigned freq)
{
	// Pre-calculate all phase-increments. The 8 different values are for
	// the 8 steps of the PM stuff (for mod and car phase calculation).
	// When PM isn't used then dphase[0] is used (pmTable[.][0] == 0).
	// The original Okazaki core calculated the PM stuff in a different
	// way. This algorithm was copied from the Burczynski core because it
	// is much more suited for a (cheap) hardware calculation.
	unsigned fnum  = freq & 511;
	unsigned block = freq / 512;
	for (int pm = 0; pm < 8; ++pm) {
		unsigned tmp = ((2 * fnum + pmTable[fnum >> 6][pm]) * patch.ML) << block;
		dphase[pm] = tmp >> (21 - DP_BITS);
	}
}

void Slot::updateTLL(unsigned freq, bool actAsCarrier)
{
	tll = patch.KL[freq >> 5] + (actAsCarrier ? volume : patch.TL);
}

void Slot::updateRKS(unsigned freq)
{
	unsigned rks = freq >> patch.KR;
	assert(rks < 16);
	dphaseDRTableRks = dphaseDRTable[rks];
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
		eg_dphase = EnvPhaseIndex::create(
			dphaseDRTableRks[patch.AR] * 12);
		break;
	case DECAY:
		eg_dphase = EnvPhaseIndex::create(dphaseDRTableRks[patch.DR]);
		break;
	case SUSTAIN:
		eg_dphase = EnvPhaseIndex::create(dphaseDRTableRks[patch.RR]);
		break;
	case RELEASE: {
		unsigned idx = sustain ? 5
		                       : (patch.EG ? patch.RR
		                                   : 7);
		eg_dphase = EnvPhaseIndex::create(dphaseDRTableRks[idx]);
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
		eg_dphase = EnvPhaseIndex::create(dphaseDRTableRks[12]);
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
	updateEG(); // EG should be updated last
}

void Slot::setEnvelopeState(EnvelopeState state_)
{
	state = state_;
	switch (state) {
	case ATTACK:
		eg_phase_max = (patch.AR == 15) ? EnvPhaseIndex(0) : EG_DP_MAX;
		break;
	case DECAY:
		eg_phase_max = EnvPhaseIndex::create(patch.SL);
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
void Slot::setPatch(const Patch& newPatch)
{
	patch = newPatch; // copy data
	if ((state == SUSHOLD) && (patch.EG == 0)) {
		setEnvelopeState(SUSTAIN);
	}
	setEnvelopeState(state); // recalc eg_phase_max
}

// Set new volume based on 4-bit register value (0-15).
void Slot::setVolume(unsigned value)
{
	// '<< 2' to transform 4 bits to the same range as the 6 bits TL field
	volume = TL2EG(value << 2);
}


//
// Channel
//

Channel::Channel()
{
	car.sibling = &mod;    // car needs a pointer to its sibling
	mod.sibling = nullptr; // mod doesn't need this pointer
}

void Channel::reset(YM2413& ym2413)
{
	mod.reset();
	car.reset();
	setPatch(0, ym2413);
}

// Change a voice
void Channel::setPatch(unsigned num, YM2413& ym2413)
{
	mod.setPatch(ym2413.getPatch(num, false));
	car.setPatch(ym2413.getPatch(num, true));
}

// Set sustain parameter
void Channel::setSustain(bool sustain, bool modActAsCarrier)
{
	car.sustain = sustain;
	if (modActAsCarrier) {
		mod.sustain = sustain;
	}
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
		patches[i][1].initCarrier  (inst_data[i]);
	}

	reset();
}

// Reset whole of OPLL except patch datas
void YM2413::reset()
{
	pm_phase = 0;
	am_phase = 0;
	noise_seed = 0xFFFF;

	for (auto& ch : channels) {
		ch.reset(*this);
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
		if (!ch7.mod.slot_on_flag) {
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

void YM2413::setRhythmFlags(byte old)
{
	Channel& ch6 = channels[6];
	Channel& ch7 = channels[7];
	Channel& ch8 = channels[8];

	// flags = X | X | mode | BD | SD | TOM | TC | HH
	byte flags = reg[0x0E];
	if ((flags ^ old) & 0x20) {
		if (flags & 0x20) {
			// OFF -> ON
			ch6.setPatch(16, *this);
			ch7.setPatch(17, *this);
			ch7.mod.setVolume(reg[0x37] >> 4);
			ch8.setPatch(18, *this);
			ch8.mod.setVolume(reg[0x38] >> 4);
		} else {
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
	}
	if (flags & 0x20) {
		if (flags & 0x10) keyOn_BD();  else keyOff_BD();
		if (flags & 0x08) keyOn_SD();  else keyOff_SD();
		if (flags & 0x04) keyOn_TOM(); else keyOff_TOM();
		if (flags & 0x02) keyOn_CYM(); else keyOff_CYM();
		if (flags & 0x01) keyOn_HH();  else keyOff_HH();
	}

	unsigned freq6 = getFreq(6);
	ch6.mod.updateAll(freq6, false);
	ch6.car.updateAll(freq6, true);
	unsigned freq7 = getFreq(7);
	ch7.mod.updateAll(freq7, isRhythm());
	ch7.car.updateAll(freq7, true);
	unsigned freq8 = getFreq(8);
	ch8.mod.updateAll(freq8, isRhythm());
	ch8.car.updateAll(freq8, true);
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
ALWAYS_INLINE unsigned Slot::calc_phase(unsigned lfo_pm)
{
	cphase += dphase[lfo_pm];
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
template<bool HAS_AM, bool FIXED_ENV>
ALWAYS_INLINE int Slot::calc_slot_car(unsigned lfo_pm, int lfo_am, int fm, unsigned fixed_env)
{
	int phase = calc_phase(lfo_pm) + wave2_8pi(fm);
	unsigned egout = calc_envelope<HAS_AM, FIXED_ENV>(lfo_am, fixed_env);
	int newOutput = dB2LinTab[patch.WF[phase & PG_MASK] + egout];
	output = (output + newOutput) >> 1;
	return output;
}

// MODULATOR
template<bool HAS_AM, bool HAS_FB, bool FIXED_ENV>
ALWAYS_INLINE int Slot::calc_slot_mod(unsigned lfo_pm, int lfo_am, unsigned fixed_env)
{
	assert((patch.FB != 0) == HAS_FB);
	unsigned phase = calc_phase(lfo_pm);
	unsigned egout = calc_envelope<HAS_AM, FIXED_ENV>(lfo_am, fixed_env);
	if (HAS_FB) {
		phase += wave2_8pi(feedback) >> patch.FB;
	}
	int newOutput = dB2LinTab[patch.WF[phase & PG_MASK] + egout];
	feedback = (output + newOutput) >> 1;
	output = newOutput;
	return feedback;
}

// TOM (ch8 mod)
ALWAYS_INLINE int Slot::calc_slot_tom()
{
	unsigned phase = calc_phase(0);
	unsigned egout = calc_envelope<false, false>(0, 0);
	return dB2LinTab[patch.WF[phase & PG_MASK] + egout];
}

// SNARE (ch7 car)
ALWAYS_INLINE int Slot::calc_slot_snare(bool noise)
{
	unsigned phase = calc_phase(0);
	unsigned egout = calc_envelope<false, false>(0, 0);
	return BIT(phase, 7)
		? dB2LinTab[(noise ? DB_POS(0.0f) : DB_POS(15.0f)) + egout]
		: dB2LinTab[(noise ? DB_NEG(0.0f) : DB_NEG(15.0f)) + egout];
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
	               ? DB_NEG(3.0f)
	               : DB_POS(3.0f);
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
	               ? (noise ? DB_NEG(12.0f) : DB_NEG(24.0f))
	               : (noise ? DB_POS(12.0f) : DB_POS(24.0f));
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

unsigned YM2413::getFreq(unsigned channel) const
{
	// combined fnum (=9bit) and block (=3bit)
	assert(channel < 9);
	return reg[0x10 + channel] | ((reg[0x20 + channel] & 0x0F) << 8);
}

Patch& YM2413::getPatch(unsigned instrument, bool carrier)
{
	return patches[instrument][carrier];
}

template <unsigned FLAGS>
ALWAYS_INLINE void YM2413::calcChannel(Channel& ch, int* buf, unsigned num)
{
	// VC++ requires explicit conversion to bool. Compiler bug??
	const bool HAS_CAR_PM = (FLAGS &  1) != 0;
	const bool HAS_CAR_AM = (FLAGS &  2) != 0;
	const bool HAS_MOD_PM = (FLAGS &  4) != 0;
	const bool HAS_MOD_AM = (FLAGS &  8) != 0;
	const bool HAS_MOD_FB = (FLAGS & 16) != 0;
	const bool HAS_CAR_FIXED_ENV = (FLAGS & 32) != 0;
	const bool HAS_MOD_FIXED_ENV = (FLAGS & 64) != 0;

	assert(((ch.car.patch.AMPM & 1) != 0) == HAS_CAR_PM);
	assert(((ch.car.patch.AMPM & 2) != 0) == HAS_CAR_AM);
	assert(((ch.mod.patch.AMPM & 1) != 0) == HAS_MOD_PM);
	assert(((ch.mod.patch.AMPM & 2) != 0) == HAS_MOD_AM);

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
		unsigned lfo_pm = 0;
		if (HAS_CAR_PM || HAS_MOD_PM) {
			// Copied from Burczynski:
			//  There are only 8 different steps for PM, and each
			//  step lasts for 1024 samples. This results in a PM
			//  freq of 6.1Hz (but datasheet says it's 6.4Hz).
			++tmp_pm_phase;
			lfo_pm = (tmp_pm_phase >> 10) & 7;
		}
		int lfo_am = 0; // avoid warning
		if (HAS_CAR_AM || HAS_MOD_AM) {
			++tmp_am_phase;
			if (tmp_am_phase == (LFO_AM_TAB_ELEMENTS * 64)) {
				tmp_am_phase = 0;
			}
			lfo_am = lfo_am_table[tmp_am_phase / 64];
		}
		int fm = ch.mod.calc_slot_mod<HAS_MOD_AM, HAS_MOD_FB, HAS_MOD_FIXED_ENV>(
		                      lfo_pm, lfo_am, mod_fixed_env);
		buf[sample] += ch.car.calc_slot_car<HAS_CAR_AM, HAS_CAR_FIXED_ENV>(
		                      lfo_pm, lfo_am, fm, car_fixed_env);
		++sample;
	} while (sample < num);
}

void YM2413::generateChannels(int* bufs[9 + 5], unsigned num)
{
	assert(num != 0);

	unsigned m = isRhythm() ? 6 : 9;
	for (unsigned i = 0; i < m; ++i) {
		Channel& ch = channels[i];
		if (ch.car.isActive()) {
			// Below we choose between 128 specialized versions of
			// calcChannel(). This allows to move a lot of
			// conditional code out of the inner-loop.
			bool carFixedEnv = (ch.car.state == SUSHOLD) ||
			                   (ch.car.state == FINISH);
			bool modFixedEnv = (ch.mod.state == SUSHOLD) ||
			                   (ch.mod.state == FINISH);
			if (ch.car.state == SETTLE) {
				modFixedEnv = false;
			}
			unsigned flags = ( ch.car.patch.AMPM     << 0) |
			                 ( ch.mod.patch.AMPM     << 2) |
			                 ((ch.mod.patch.FB != 0) << 4) |
			                 ( carFixedEnv           << 5) |
			                 ( modFixedEnv           << 6);
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
		} else {
			bufs[i] = nullptr;
		}
	}
	// update AM, PM unit
	pm_phase += num;
	am_phase = (am_phase + num) % (LFO_AM_TAB_ELEMENTS * 64);

	if (isRhythm()) {
		bufs[6] = nullptr;
		bufs[7] = nullptr;
		bufs[8] = nullptr;

		Channel& ch6 = channels[6];
		Channel& ch7 = channels[7];
		Channel& ch8 = channels[8];

		unsigned old_noise = noise_seed;
		unsigned old_cphase7 = ch7.mod.cphase;
		unsigned old_cphase8 = ch8.car.cphase;

		if (ch6.car.isActive()) {
			for (unsigned sample = 0; sample < num; ++sample) {
				bufs[ 9][sample] += 2 *
				    ch6.car.calc_slot_car<false, false>(
				        0, 0, ch6.mod.calc_slot_mod<
				                false, false, false>(0, 0, 0), 0);
			}
		} else {
			bufs[9] = nullptr;
		}

		if (ch7.car.isActive()) {
			for (unsigned sample = 0; sample < num; ++sample) {
				noise_seed >>= 1;
				bool noise_bit = noise_seed & 1;
				if (noise_bit) noise_seed ^= 0x8003020;
				bufs[10][sample] +=
					-2 * ch7.car.calc_slot_snare(noise_bit);
			}
		} else {
			bufs[10] = nullptr;
		}

		if (ch8.car.isActive()) {
			for (unsigned sample = 0; sample < num; ++sample) {
				unsigned phase7 = ch7.mod.calc_phase(0);
				unsigned phase8 = ch8.car.calc_phase(0);
				bufs[11][sample] +=
					-2 * ch8.car.calc_slot_cym(phase7, phase8);
			}
		} else {
			bufs[11] = nullptr;
		}

		if (ch7.mod.isActive()) {
			// restore noise, ch7/8 cphase
			noise_seed = old_noise;
			ch7.mod.cphase = old_cphase7;
			ch8.car.cphase = old_cphase8;
			for (unsigned sample = 0; sample < num; ++sample) {
				noise_seed >>= 1;
				bool noise_bit = noise_seed & 1;
				if (noise_bit) noise_seed ^= 0x8003020;
				unsigned phase7 = ch7.mod.calc_phase(0);
				unsigned phase8 = ch8.car.calc_phase(0);
				bufs[12][sample] +=
					2 * ch7.mod.calc_slot_hat(phase7, phase8, noise_bit);
			}
		} else {
			bufs[12] = nullptr;
		}

		if (ch8.mod.isActive()) {
			for (unsigned sample = 0; sample < num; ++sample) {
				bufs[13][sample] += 2 * ch8.mod.calc_slot_tom();
			}
		} else {
			bufs[13] = nullptr;
		}
	} else {
		bufs[ 9] = nullptr;
		bufs[10] = nullptr;
		bufs[11] = nullptr;
		bufs[12] = nullptr;
		bufs[13] = nullptr;
	}
}

void YM2413::writeReg(byte r, byte data)
{
	assert(r < 0x40);

	switch (r) {
	case 0x00: {
		reg[r] = data;
		patches[0][0].AMPM = (data >> 6) & 3;
		patches[0][0].EG   = (data >> 5) & 1;
		patches[0][0].setKR ((data >> 4) & 1);
		patches[0][0].setML ((data >> 0) & 15);
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				unsigned freq = getFreq(i);
				ch.mod.updatePG (freq);
				ch.mod.updateRKS(freq);
				ch.mod.updateEG();
			}
		}
		break;
	}
	case 0x01: {
		reg[r] = data;
		patches[0][1].AMPM = (data >> 6) & 3;
		patches[0][1].EG   = (data >> 5) & 1;
		patches[0][1].setKR ((data >> 4) & 1);
		patches[0][1].setML ((data >> 0) & 15);
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				unsigned freq = getFreq(i);
				ch.car.updatePG (freq);
				ch.car.updateRKS(freq);
				ch.car.updateEG();
			}
		}
		break;
	}
	case 0x02: {
		reg[r] = data;
		patches[0][0].setKL((data >> 6) &  3);
		patches[0][0].setTL((data >> 0) & 63);
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				bool actAsCarrier = (i >= 7) && isRhythm();
				assert(!actAsCarrier); (void)actAsCarrier;
				ch.mod.updateTLL(getFreq(i), false);
			}
		}
		break;
	}
	case 0x03: {
		reg[r] = data;
		patches[0][1].setKL((data >> 6) & 3);
		patches[0][1].setWF((data >> 4) & 1);
		patches[0][0].setWF((data >> 3) & 1);
		patches[0][0].setFB((data >> 0) & 7);
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
			}
		}
		break;
	}
	case 0x04: {
		reg[r] = data;
		patches[0][0].AR = (data >> 4) & 15;
		patches[0][0].DR = (data >> 0) & 15;
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				ch.mod.updateEG();
				if (ch.mod.state == ATTACK) {
					ch.mod.setEnvelopeState(ATTACK);
				}
			}
		}
		break;
	}
	case 0x05: {
		reg[r] = data;
		patches[0][1].AR = (data >> 4) & 15;
		patches[0][1].DR = (data >> 0) & 15;
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				ch.car.updateEG();
				if (ch.car.state == ATTACK) {
					ch.car.setEnvelopeState(ATTACK);
				}
			}
		}
		break;
	}
	case 0x06: {
		reg[r] = data;
		patches[0][0].setSL((data >> 4) & 15);
		patches[0][0].RR  = (data >> 0) & 15;
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; ++i) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				ch.mod.updateEG();
				if (ch.mod.state == DECAY) {
					ch.mod.setEnvelopeState(DECAY);
				}
			}
		}
		break;
	}
	case 0x07: {
		reg[r] = data;
		patches[0][1].setSL((data >> 4) & 15);
		patches[0][1].RR  = (data >> 0) & 15;
		unsigned m = isRhythm() ? 6 : 9;
		for (unsigned i = 0; i < m; i++) {
			if ((reg[0x30 + i] & 0xF0) == 0) {
				Channel& ch = channels[i];
				ch.setPatch(0, *this); // TODO optimize
				ch.car.updateEG();
				if (ch.car.state == DECAY) {
					ch.car.setEnvelopeState(DECAY);
				}
			}
		}
		break;
	}
	case 0x0E: {
		byte old = reg[r];
		reg[r] = data;
		setRhythmFlags(old);
		break;
	}
	case 0x19: case 0x1A: case 0x1B: case 0x1C:
	case 0x1D: case 0x1E: case 0x1F:
		r -= 9; // verified on real YM2413
		// fall-through
	case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
	case 0x15: case 0x16: case 0x17: case 0x18: {
		reg[r] = data;
		unsigned cha = r & 0x0F; assert(cha < 9);
		Channel& ch = channels[cha];
		bool actAsCarrier = (cha >= 7) && isRhythm();
		unsigned freq = getFreq(cha);
		ch.mod.updateAll(freq, actAsCarrier);
		ch.car.updateAll(freq, true);
		break;
	}
	case 0x29: case 0x2A: case 0x2B: case 0x2C:
	case 0x2D: case 0x2E: case 0x2F:
		r -= 9; // verified on real YM2413
		// fall-through
	case 0x20: case 0x21: case 0x22: case 0x23: case 0x24:
	case 0x25: case 0x26: case 0x27: case 0x28: {
		reg[r] = data;
		unsigned cha = r & 0x0F; assert(cha < 9);
		Channel& ch = channels[cha];
		bool modActAsCarrier = (cha >= 7) && isRhythm();
		ch.setSustain((data >> 5) & 1, modActAsCarrier);
		if (data & 0x10) {
			ch.keyOn();
		} else {
			ch.keyOff();
		}
		unsigned freq = getFreq(cha);
		ch.mod.updateAll(freq, modActAsCarrier);
		ch.car.updateAll(freq, true);
		break;
	}
	case 0x39: case 0x3A: case 0x3B: case 0x3C:
	case 0x3D: case 0x3E: case 0x3F:
		r -= 9; // verified on real YM2413
		// fall-through
	case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
	case 0x35: case 0x36: case 0x37: case 0x38: {
		reg[r] = data;
		unsigned cha = r & 0x0F; assert(cha < 9);
		Channel& ch = channels[cha];
		if (isRhythm() && (cha >= 6)) {
			if (cha > 6) {
				// channel 7 or 8 in ryhthm mode
				ch.mod.setVolume(data >> 4);
			}
		} else {
			ch.setPatch(data >> 4, *this);
		}
		ch.car.setVolume(data & 15);
		bool actAsCarrier = (cha >= 7) && isRhythm();
		unsigned freq = getFreq(cha);
		ch.mod.updateAll(freq, actAsCarrier);
		ch.car.updateAll(freq, true);
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

static std::initializer_list<enum_string<YM2413Okazaki::EnvelopeState>> envelopeStateInfo = {
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

// version 1: initial version
// version 2: don't serialize "type / actAsCarrier" anymore, it's now
//            a calculated value
// version 3: don't serialize slot_on_flag anymore
// version 4: don't serialize volume anymore
template<typename Archive>
void Slot::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("feedback", feedback);
	ar.serialize("output", output);
	ar.serialize("cphase", cphase);
	ar.serialize("state", state);
	ar.serialize("eg_phase", eg_phase);
	ar.serialize("sustain", sustain);

	// These are restored by calls to
	//  updateAll():         eg_dphase, dphaseDRTableRks, tll, dphase
	//  setEnvelopeState():  eg_phase_max
	//  setPatch():          patch
	//  setVolume():         volume
	//  update_key_status(): slot_on_flag
}

// version 1: initial version
// version 2: removed patch_number, freq
template<typename Archive>
void Channel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mod", mod);
	ar.serialize("car", car);
}


// version 1: initial version
// version 2: 'registers' are moved here (no longer serialized in base class)
// version 3: no longer serialize 'user_patch_mod' and 'user_patch_car'
template<typename Archive>
void YM2413::serialize(Archive& ar, unsigned version)
{
	if (ar.versionBelow(version, 2)) ar.beginTag("YM2413Core");
	ar.serialize("registers", reg);
	if (ar.versionBelow(version, 2)) ar.endTag("YM2413Core");

	// no need to serialize patches[]
	//   patches[0] is restored from registers, the others are read-only
	ar.serialize("channels", channels);
	ar.serialize("pm_phase", pm_phase);
	ar.serialize("am_phase", am_phase);
	ar.serialize("noise_seed", noise_seed);

	if (ar.isLoader()) {
		patches[0][0].initModulator(&reg[0]);
		patches[0][1].initCarrier  (&reg[0]);
		for (int i = 0; i < 9; ++i) {
			Channel& ch = channels[i];
			// restore patch
			unsigned p = ((i >= 6) && isRhythm())
			           ? (16 + (i - 6))
			           : (reg[0x30 + i] >> 4);
			ch.setPatch(p, *this); // before updateAll()
			// restore volume
			ch.car.setVolume(reg[0x30 + i] & 15);
			if (isRhythm() && (i >= 7)) { // ch 7/8 ryhthm
				ch.mod.setVolume(reg[0x30 + i] >> 4);
			}
			// sync various variables
			bool actAsCarrier = (i >= 7) && isRhythm();
			unsigned freq = getFreq(i);
			ch.mod.updateAll(freq, actAsCarrier);
			ch.car.updateAll(freq, true);
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
