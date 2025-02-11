// Based on ymf278b.c written by R. Belmont and O. Galibert

// Improved by Valley Bell, 2018
// Thanks to niekniek and l_oliveira for providing recordings from OPL4 hardware.
// Thanks to superctr and wouterv for discussing changes.
//
// Improvements:
// - added TL interpolation, recordings show that internal TL levels are 0x00..0xff
// - fixed ADSR speeds, attack rate 15 is now instant
// - correct clamping of intermediate Rate Correction values
// - emulation of "loop glitch" (going out-of-bounds by playing a sample faster than it the loop is long)
// - made calculation of sample position cleaner and closer to how the HW works
// - increased output resolution from TL (0.375dB) to envelope (0.09375dB)
// - fixed volume table -6dB steps are done using bit shifts, steps in between are multiplicators
// - made octave -8 freeze the sample
// - verified that TL and envelope levels are applied separately, both go silent at -60dB
// - implemented pseudo-reverb and damping according to manual
// - made pseudo-reverb ignore Rate Correction (real hardware ignores it)
// - reimplemented LFO, speed exactly matches the formulas that were probably used when creating the manual
// - fixed LFO (tremolo) amplitude modulation
// - made LFO vibrato and tremolo accurate to hardware
//
// Known issues:
// - Octave -8 was only tested with fnum 0. Other fnum values might behave differently.

// This class doesn't model a full YMF278B chip. Instead it only models the
// wave part. The FM part in modeled in YMF262 (it's almost 100% compatible,
// the small differences are handled in YMF262). The status register and
// interaction with the FM registers (e.g. the NEW2 bit) is handled in the
// YMF278B class.

#include "YMF278.hh"

#include "DeviceConfig.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "enumerate.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "outer.hh"
#include "xrange.hh"

#include <algorithm>

namespace openmsx {

// envelope output entries
// fixed to match recordings from actual OPL4 -Valley Bell
static constexpr int MAX_ATT_INDEX = 0x280; // makes attack phase right and also goes well with "envelope stops at -60dB"
static constexpr int MIN_ATT_INDEX = 0;
static constexpr int TL_SHIFT      = 2; // envelope values are 4x as fine as TL levels

static constexpr unsigned LFO_SHIFT = 18; // LFO period of up to 0x40000 sample
static constexpr unsigned LFO_PERIOD = 1 << LFO_SHIFT;

// Envelope Generator phases
static constexpr int EG_ATT = 4;
static constexpr int EG_DEC = 3;
static constexpr int EG_SUS = 2;
static constexpr int EG_REL = 1;
static constexpr int EG_OFF = 0;
// these 2 are only used in old savestates (and are converted to EG_REL on load)
static constexpr int EG_REV = 5; // pseudo reverb
static constexpr int EG_DMP = 6; // damp

// Pan values, units are -3dB, i.e. 8.
static constexpr std::array<uint8_t, 16> pan_left = {
	0, 8, 16, 24, 32, 40, 48, 255, 255,   0,  0,  0,  0,  0,  0, 0
};
static constexpr std::array<uint8_t, 16> pan_right = {
	0, 0,  0,  0,  0,  0,  0,   0, 255, 255, 48, 40, 32, 24, 16, 8
};

// decay level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
static constexpr int16_t SC(int dB) { return int16_t(dB / 3 * 0x20); }
static constexpr std::array<int16_t, 16> dl_tab = {
 SC( 0), SC( 3), SC( 6), SC( 9), SC(12), SC(15), SC(18), SC(21),
 SC(24), SC(27), SC(30), SC(33), SC(36), SC(39), SC(42), SC(93)
};

static constexpr uint8_t RATE_STEPS = 8;
static constexpr std::array<uint8_t, 15 * RATE_STEPS> eg_inc = {
//cycle:0  1   2  3   4  5   6  7
	0, 1,  0, 1,  0, 1,  0, 1, //  0  rates 00..12 0 (increment by 0 or 1)
	0, 1,  0, 1,  1, 1,  0, 1, //  1  rates 00..12 1
	0, 1,  1, 1,  0, 1,  1, 1, //  2  rates 00..12 2
	0, 1,  1, 1,  1, 1,  1, 1, //  3  rates 00..12 3

	1, 1,  1, 1,  1, 1,  1, 1, //  4  rate 13 0 (increment by 1)
	1, 1,  1, 2,  1, 1,  1, 2, //  5  rate 13 1
	1, 2,  1, 2,  1, 2,  1, 2, //  6  rate 13 2
	1, 2,  2, 2,  1, 2,  2, 2, //  7  rate 13 3

	2, 2,  2, 2,  2, 2,  2, 2, //  8  rate 14 0 (increment by 2)
	2, 2,  2, 4,  2, 2,  2, 4, //  9  rate 14 1
	2, 4,  2, 4,  2, 4,  2, 4, // 10  rate 14 2
	2, 4,  4, 4,  2, 4,  4, 4, // 11  rate 14 3

	4, 4,  4, 4,  4, 4,  4, 4, // 12  rates 15 0, 15 1, 15 2, 15 3 for decay
	8, 8,  8, 8,  8, 8,  8, 8, // 13  rates 15 0, 15 1, 15 2, 15 3 for attack (zero time)
	0, 0,  0, 0,  0, 0,  0, 0, // 14  infinity rates for attack and decay(s)
};

[[nodiscard]] static constexpr uint8_t O(int a) { return narrow<uint8_t>(a * RATE_STEPS); }
static constexpr std::array<uint8_t, 64> eg_rate_select = {
	O(14),O(14),O(14),O(14), // inf rate
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 4),O( 5),O( 6),O( 7),
	O( 8),O( 9),O(10),O(11),
	O(12),O(12),O(12),O(12),
};

// rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
// shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
// mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0
static constexpr std::array<uint8_t, 64> eg_rate_shift = {
	 12, 12, 12, 12,
	 11, 11, 11, 11,
	 10, 10, 10, 10,
	  9,  9,  9,  9,
	  8,  8,  8,  8,
	  7,  7,  7,  7,
	  6,  6,  6,  6,
	  5,  5,  5,  5,
	  4,  4,  4,  4,
	  3,  3,  3,  3,
	  2,  2,  2,  2,
	  1,  1,  1,  1,
	  0,  0,  0,  0,
	  0,  0,  0,  0,
	  0,  0,  0,  0,
	  0,  0,  0,  0,
};


// number of steps the LFO counter advances per sample
//   LFO frequency (Hz) -> LFO counter steps per sample
[[nodiscard]] static constexpr int L(double a) { return int((LFO_PERIOD * a) / 44100.0 + 0.5); }
static constexpr std::array<int, 8> lfo_period = {
	L(0.168), // step:  1, period: 262144 samples
	L(2.019), // step: 12, period:  21845 samples
	L(3.196), // step: 19, period:  13797 samples
	L(4.206), // step: 25, period:  10486 samples
	L(5.215), // step: 31, period:   8456 samples
	L(5.888), // step: 35, period:   7490 samples
	L(6.224), // step: 37, period:   7085 samples
	L(7.066), // step: 42, period:   6242 samples
};


// formula used by Yamaha docs:
//     vib_depth_cents(x) = (log2(0x400 + x) - 10) * 1200
static constexpr std::array<int16_t, 8> vib_depth = {
	0,      //  0.000 cents
	2,      //  3.378 cents
	3,      //  5.065 cents
	4,      //  6.750 cents
	6,      // 10.114 cents
	12,     // 20.170 cents
	24,     // 40.106 cents
	48,     // 79.307 cents
};


// formula used by Yamaha docs:
//     am_depth_db(x) = (x-1) / 0x40 * 6.0
//     They use (x-1), because the depth is multiplied with the AM counter, which has a range of 0..0x7F.
//     Thus the maximum attenuation with x=0x80 is (0x7F * 0x80) >> 7 = 0x7F.
// reversed formula:
//     am_depth(dB) = round(dB / 6.0 * 0x40) + 1
static constexpr std::array<uint8_t, 8> am_depth = {
	0x00,   //  0.000 dB
	0x14,   //  1.781 dB
	0x20,   //  2.906 dB
	0x28,   //  3.656 dB
	0x30,   //  4.406 dB
	0x40,   //  5.906 dB
	0x50,   //  7.406 dB
	0x80,   // 11.910 dB
};


YMF278::Slot::Slot()
{
	reset();
}

// Sign extend a 4-bit value to int8_t
// require: x in range [0..15]
[[nodiscard]] static constexpr int8_t sign_extend_4(int x)
{
	return narrow<int8_t>((x ^ 8) - 8);
}

// Params: oct in [-8 ..   +7]
//         fn  in [ 0 .. 1023]
// We want to interpret oct as a signed 4-bit number and calculate
//    ((fn | 1024) + vib) << (5 + sign_extend_4(oct))
// Though in this formula the shift can go over a negative distance (in that
// case we should shift in the other direction).
[[nodiscard]] static constexpr unsigned calcStep(int8_t oct, uint16_t fn, int16_t vib = 0)
{
	if (oct == -8) return 0;
	unsigned t = (fn + 1024 + vib) << (8 + oct); // use '+' iso '|' (generates slightly better code)
	return t >> 3; // was shifted 3 positions too far
}

void YMF278::Slot::reset()
{
	wave = FN = TLdest = TL = pan = vib = AM = 0;
	OCT = 0;
	DL = 0;
	AR = D1R = D2R = RC = RR = 0;
	PRVB = keyon = DAMP = false;
	stepPtr = 0;
	step = calcStep(OCT, FN);
	bits = 0;
	startAddr = 0;
	loopAddr = endAddr = 0;
	env_vol = MAX_ATT_INDEX;

	lfo_active = false;
	lfo_cnt = 0;
	lfo = 0;

	state = EG_OFF;

	// not strictly needed, but avoid UMR on savestate
	pos = 0;
}

uint8_t YMF278::Slot::compute_rate(int val) const
{
	if (val == 0) {
		return 0;
	} else if (val == 15) {
		return 63;
	}
	int res = val * 4;
	if (RC != 15) {
		// clamping verified with HW tests -Valley Bell
		res += 2 * std::clamp(OCT + RC, 0, 15);
		res += (FN & 0x200) ? 1 : 0;
	}
	return narrow<uint8_t>(std::clamp(res, 0, 63));
}

uint8_t YMF278::Slot::compute_decay_rate(int val) const
{
	if (DAMP) {
		// damping
		// The manual lists these values for time and attenuation: (44100 samples/second)
		// -12dB at  5.8ms, sample 256
		// -48dB at  8.0ms, sample 352
		// -72dB at  9.4ms, sample 416
		// -96dB at 10.9ms, sample 480
		// This results in these durations and rate values for the respective phases:
		//   0dB .. -12dB: 256 samples (5.80ms) -> 128 samples per -6dB = rate 48
		// -12dB .. -48dB:  96 samples (2.18ms) ->  16 samples per -6dB = rate 63
		// -48dB .. -72dB:  64 samples (1.45ms) ->  16 samples per -6dB = rate 63
		// -72dB .. -96dB:  64 samples (1.45ms) ->  16 samples per -6dB = rate 63
		// Damping was verified to ignore rate correction.
		if (env_vol < dl_tab[4]) {
			return 48; //   0dB .. -12dB
		} else {
			return 63; // -12dB .. -96dB
		}
	}
	if (PRVB) {
		// pseudo reverb
		// activated when reaching -18dB, overrides D1R/D2R/RR with reverb rate 5
		//
		// The manual is actually a bit unclear and just says "RATE=5",
		// referring to the D1R/D2R/RR register value. However, later
		// pages use "RATE" to refer to the "internal" rate, which is
		// (register * 4) + rate correction. HW recordings prove that
		// Rate Correction is ignored, so pseudo reverb just sets the
		// "internal" rate to a value of 4*5 = 20.
		if (env_vol >= dl_tab[6]) {
			return 20;
		}
	}
	return compute_rate(val);
}

int16_t YMF278::Slot::compute_vib() const
{
	// verified via hardware recording:
	//  With LFO speed 0 (period 262144 samples), each vibrato step takes
	//  4096 samples.
	//  -> 64 steps total
	//  Also, with vibrato depth 7 (80 cents) and an F-Num of 0x400, the
	//  final F-Nums are: 0x400 .. 0x43C, 0x43C .. 0x400, 0x400 .. 0x3C4,
	//  0x3C4 .. 0x400
	auto lfo_fm = narrow<int16_t>(lfo_cnt / (LFO_PERIOD / 0x40));
	// results in +0x00..+0x0F, +0x0F..+0x00, -0x00..-0x0F, -0x0F..-0x00
	if (lfo_fm & 0x10) lfo_fm ^= 0x1F;
	if (lfo_fm & 0x20) lfo_fm = narrow<int16_t>(-(lfo_fm & 0x0F));

	return narrow<int16_t>((lfo_fm * vib_depth[vib]) / 12);
}

uint16_t YMF278::Slot::compute_am() const
{
	// verified via hardware recording:
	//  With LFO speed 0 (period 262144 samples), each tremolo step takes
	//  1024 samples.
	//  -> 256 steps total
	auto lfo_am = narrow<uint16_t>(lfo_cnt / (LFO_PERIOD / 0x100));
	// results in 0x00..0x7F, 0x7F..0x00
	if (lfo_am >= 0x80) lfo_am ^= 0xFF;

	return narrow<uint16_t>((lfo_am * am_depth[AM]) >> 7);
}


void YMF278::advance()
{
	eg_cnt++;

	// modulo counters for volume interpolation
	auto tl_int_cnt  =  eg_cnt % 9;      // 0 .. 8
	auto tl_int_step = (eg_cnt / 9) % 3; // 0 .. 2

	for (auto& op : slots) {
		// volume interpolation
		if (tl_int_cnt == 0) {
			if (tl_int_step == 0) {
				// decrease volume by one step every 27 samples
				if (op.TL < op.TLdest) ++op.TL;
			} else {
				// increase volume by one step every 13.5 samples
				if (op.TL > op.TLdest) --op.TL;
			}
		}

		if (op.lfo_active) {
			op.lfo_cnt = (op.lfo_cnt + lfo_period[op.lfo]) & (LFO_PERIOD - 1);
		}

		// Envelope Generator
		switch (op.state) {
		case EG_ATT: { // attack phase
			uint8_t rate = op.compute_rate(op.AR);
			// Verified by HW recording (and matches Nemesis' tests of the YM2612):
			// AR = 0xF during KeyOn results in instant switch to EG_DEC. (see keyOnHelper)
			// Setting AR = 0xF while the attack phase is in progress freezes the envelope.
			if (rate >= 63) {
				break;
			}
			uint8_t shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) - 1))) {
				uint8_t select = eg_rate_select[rate];
				// >>4 makes the attack phase's shape match the actual chip -Valley Bell
				op.env_vol = narrow<int16_t>(op.env_vol + ((~op.env_vol * eg_inc[select + ((eg_cnt >> shift) & 7)]) >> 4));
				if (op.env_vol <= MIN_ATT_INDEX) {
					op.env_vol = MIN_ATT_INDEX;
					// TODO does the real HW skip EG_DEC completely,
					//      or is it active for 1 sample?
					op.state = op.DL ? EG_DEC : EG_SUS;
				}
			}
			break;
		}
		case EG_DEC: { // decay phase
			uint8_t rate = op.compute_decay_rate(op.D1R);
			uint8_t shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) - 1))) {
				uint8_t select = eg_rate_select[rate];
				op.env_vol = narrow<int16_t>(op.env_vol + eg_inc[select + ((eg_cnt >> shift) & 7)]);
				if (op.env_vol >= op.DL) {
					op.state = (op.env_vol < MAX_ATT_INDEX) ? EG_SUS : EG_OFF;
				}
			}
			break;
		}
		case EG_SUS: { // sustain phase
			uint8_t rate = op.compute_decay_rate(op.D2R);
			uint8_t shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) - 1))) {
				uint8_t select = eg_rate_select[rate];
				op.env_vol = narrow<int16_t>(op.env_vol + eg_inc[select + ((eg_cnt >> shift) & 7)]);
				if (op.env_vol >= MAX_ATT_INDEX) {
					op.env_vol = MAX_ATT_INDEX;
					op.state = EG_OFF;
				}
			}
			break;
		}
		case EG_REL: { // release phase
			uint8_t rate = op.compute_decay_rate(op.RR);
			uint8_t shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) - 1))) {
				uint8_t select = eg_rate_select[rate];
				op.env_vol = narrow<int16_t>(op.env_vol + eg_inc[select + ((eg_cnt >> shift) & 7)]);
				if (op.env_vol >= MAX_ATT_INDEX) {
					op.env_vol = MAX_ATT_INDEX;
					op.state = EG_OFF;
				}
			}
			break;
		}
		case EG_OFF:
			// nothing
			break;

		default:
			UNREACHABLE;
		}
	}
}

int16_t YMF278::getSample(const Slot& slot, uint16_t pos) const
{
	// TODO How does this behave when R#2 bit 0 = 1?
	//      As-if read returns 0xff? (Like for CPU memory reads.) Or is
	//      sound generation blocked at some higher level?
	switch (slot.bits) {
	case 0: {
		// 8 bit
		return narrow_cast<int16_t>(readMem(slot.startAddr + pos) << 8);
	}
	case 1: {
		// 12 bit
		unsigned addr = slot.startAddr + ((pos / 2) * 3);
		if (pos & 1) {
			return narrow_cast<int16_t>(
				(readMem(addr + 2) << 8) |
				(readMem(addr + 1) & 0xF0));
		} else {
			return narrow_cast<int16_t>(
				 (readMem(addr + 0) << 8) |
				((readMem(addr + 1) << 4) & 0xF0));
		}
	}
	case 2: {
		// 16 bit
		unsigned addr = slot.startAddr + (pos * 2);
		return narrow_cast<int16_t>(
			(readMem(addr + 0) << 8) |
			(readMem(addr + 1) << 0));
	}
	default:
		// TODO unspecified
		return 0;
	}
}

uint16_t YMF278::nextPos(const Slot& slot, uint16_t pos, uint16_t increment)
{
	// If there is a 4-sample loop and you advance 12 samples per step,
	// it may exceed the end offset.
	// This is abused by the "Lizard Star" song to generate noise at 0:52. -Valley Bell
	pos += increment;
	if ((uint32_t(pos) + slot.endAddr) >= 0x10000) // check position >= (negated) end address
		pos += narrow_cast<uint16_t>(slot.endAddr + slot.loopAddr); // This is how the actual chip does it.
	return pos;
}

bool YMF278::anyActive()
{
	return std::ranges::any_of(slots, [](auto& op) { return op.state != EG_OFF; });
}

// In: 'envVol', 0=max volume, others -> -3/32 = -0.09375 dB/step
// Out: 'x' attenuated by the corresponding factor.
// Note: microbenchmarks have shown that re-doing this calculation is about the
// same speed as using a 4kB lookup table.
static constexpr int vol_factor(int x, unsigned envVol)
{
	if (envVol >= MAX_ATT_INDEX) return 0; // hardware clips to silence below -60dB
	int vol_mul = 0x80 - narrow<int>(envVol & 0x3F); // 0x40 values per 6dB
	int vol_shift = 7 + narrow<int>(envVol >> 6);
	return (x * ((0x8000 * vol_mul) >> vol_shift)) >> 15;
}

void YMF278::setMixLevel(uint8_t x, EmuTime::param time)
{
	static constexpr std::array<float, 8> level = {
		(1.00f / 1), //   0dB
		(0.75f / 1), //  -3dB (approx)
		(1.00f / 2), //  -6dB
		(0.75f / 2), //  -9dB (approx)
		(1.00f / 4), // -12dB
		(0.75f / 4), // -15dB (approx)
		(1.00f / 8), // -18dB
		 0.00f       // -inf dB
	};
	setSoftwareVolume(level[x & 7], level[(x >> 3) & 7], time);
}

void YMF278::generateChannels(std::span<float*> bufs, unsigned num)
{
	if (!anyActive()) {
		// TODO update internal state, even if muted
		// TODO also mute individual channels
		std::ranges::fill(bufs, nullptr);
		return;
	}

	for (auto j : xrange(num)) {
		for (auto i : xrange(24)) {
			auto& sl = slots[i];
			if (sl.state == EG_OFF) {
				//bufs[i][2 * j + 0] += 0;
				//bufs[i][2 * j + 1] += 0;
				continue;
			}

			auto sample = narrow_cast<int16_t>(
				(getSample(sl, sl.pos) * (0x10000 - sl.stepPtr) +
				 getSample(sl, nextPos(sl, sl.pos, 1)) * sl.stepPtr) >> 16);
			// TL levels are 00..FF internally (TL register value 7F is mapped to TL level FF)
			// Envelope levels have 4x the resolution (000..3FF)
			// Volume levels are approximate logarithmic. -6dB result in half volume. Steps in between use linear interpolation.
			// A volume of -60dB or lower results in silence. (value 0x280..0x3FF).
			// Recordings from actual hardware indicate that TL level and envelope level are applied separately.
			// Each of them is clipped to silence below -60dB, but TL+envelope might result in a lower volume. -Valley Bell
			auto envVol = narrow_cast<uint16_t>(
				std::min(sl.env_vol + ((sl.lfo_active && sl.AM) ? sl.compute_am() : 0),
				         MAX_ATT_INDEX));
			int smplOut = vol_factor(vol_factor(sample, envVol), sl.TL << TL_SHIFT);

			// Panning is also done separately. (low-volume TL + low-volume panning goes below -60dB)
			// I'll be taking wild guess and assume that -3dB is approximated with 75%. (same as with TL and envelope levels)
			// The same applies to the PCM mix level.
			int32_t volLeft  = pan_left [sl.pan]; // note: register 0xF9 is handled externally
			int32_t volRight = pan_right[sl.pan];
			// 0 -> 0x20, 8 -> 0x18, 16 -> 0x10, 24 -> 0x0C, etc. (not using vol_factor here saves array boundary checks)
			volLeft  = (0x20 - (volLeft  & 0x0f)) >> (volLeft  >> 4);
			volRight = (0x20 - (volRight & 0x0f)) >> (volRight >> 4);

			bufs[i][2 * j + 0] += narrow_cast<float>((smplOut * volLeft ) >> 5);
			bufs[i][2 * j + 1] += narrow_cast<float>((smplOut * volRight) >> 5);

			unsigned step = (sl.lfo_active && sl.vib)
			              ? calcStep(sl.OCT, sl.FN, sl.compute_vib())
			              : sl.step;
			sl.stepPtr += step;

			if (sl.stepPtr >= 0x10000) {
				sl.pos = nextPos(sl, sl.pos, narrow<uint16_t>(sl.stepPtr >> 16));
				sl.stepPtr &= 0xffff;
			}
		}
		advance();
	}
}

void YMF278::keyOnHelper(YMF278::Slot& slot) const
{
	// Unlike FM, the envelope level is reset. (And it makes sense, because you restart the sample.)
	slot.env_vol = MAX_ATT_INDEX;
	if (slot.compute_rate(slot.AR) < 63) {
		slot.state = EG_ATT;
	} else {
		// Nuke.YKT verified that the FM part does it exactly this way,
		// and the OPL4 manual says it's instant as well.
		slot.env_vol = MIN_ATT_INDEX;
		// see comment in 'case EG_ATT' in YMF278::advance()
		slot.state = slot.DL ? EG_DEC : EG_SUS;
	}
	slot.stepPtr = 0;
	slot.pos = 0;
}

void YMF278::writeReg(uint8_t reg, uint8_t data, EmuTime::param time)
{
	updateStream(time); // TODO optimize only for regs that directly influence sound
	writeRegDirect(reg, data, time);
}

void YMF278::writeRegDirect(uint8_t reg, uint8_t data, EmuTime::param time)
{
	// Handle slot registers specifically
	if (reg >= 0x08 && reg <= 0xF7) {
		int sNum = (reg - 8) % 24;
		auto& slot = slots[sNum];
		switch ((reg - 8) / 24) {
		case 0: {
			slot.wave = (slot.wave & 0x100) | data;
			int waveTblHdr = (regs[2] >> 2) & 0x7;
			int base = (slot.wave < 384 || !waveTblHdr) ?
			           (slot.wave * 12) :
			           (waveTblHdr * 0x80000 + ((slot.wave - 384) * 12));
			std::array<uint8_t, 12> buf;
			for (auto i : xrange(12)) {
				// TODO What if R#2 bit 0 = 1?
				//      See also getSample()
				buf[i] = readMem(base + i);
			}
			slot.bits = (buf[0] & 0xC0) >> 6;
			slot.startAddr = buf[2] | (buf[1] << 8) | ((buf[0] & 0x3F) << 16);
			slot.loopAddr = uint16_t(buf[4] | (buf[3] << 8));
			slot.endAddr  = uint16_t(buf[6] | (buf[5] << 8));
			for (auto i : xrange(7, 12)) {
				// Verified on real YMF278:
				// After tone loading, if you read these
				// registers, their value actually has changed.
				writeRegDirect(narrow<uint8_t>(8 + sNum + (i - 2) * 24), buf[i], time);
			}
			if (slot.keyon) {
				keyOnHelper(slot);
			} else {
				slot.stepPtr = 0;
				slot.pos = 0;
			}
			break;
		}
		case 1: {
			slot.wave = uint16_t((slot.wave & 0xFF) | ((data & 0x1) << 8));
			slot.FN = (slot.FN & 0x380) | (data >> 1);
			slot.step = calcStep(slot.OCT, slot.FN);
			break;
		}
		case 2: {
			slot.FN = uint16_t((slot.FN & 0x07F) | ((data & 0x07) << 7));
			slot.PRVB = (data & 0x08) != 0;
			slot.OCT = sign_extend_4((data & 0xF0) >> 4);
			slot.step = calcStep(slot.OCT, slot.FN);
			break;
		}
		case 3: {
			uint8_t t = data >> 1;
			slot.TLdest = (t != 0x7f) ? t : 0xff; // verified on HW via volume interpolation
			if (data & 1) {
				// directly change volume
				slot.TL = slot.TLdest;
			} else {
				// interpolate volume
			}
			break;
		}
		case 4:
			if (data & 0x10) {
				// output to DO1 pin:
				// this pin is not used in MoonSound
				// we emulate this by muting the sound
				slot.pan = 8; // both left/right -inf dB
			} else {
				slot.pan = data & 0x0F;
			}

			if (data & 0x20) {
				// LFO reset
				slot.lfo_active = false;
				slot.lfo_cnt = 0;
			} else {
				// LFO activate
				slot.lfo_active = true;
			}

			slot.DAMP = (data & 0x40) != 0;

			if (data & 0x80) {
				if (!slot.keyon) {
					slot.keyon = true;
					keyOnHelper(slot);
				}
			} else {
				if (slot.keyon) {
					slot.keyon = false;
					slot.state = EG_REL;
				}
			}
			break;
		case 5:
			slot.lfo = (data >> 3) & 0x7;
			slot.vib = data & 0x7;
			break;
		case 6:
			slot.AR  = data >> 4;
			slot.D1R = data & 0xF;
			break;
		case 7:
			slot.DL  = dl_tab[data >> 4];
			slot.D2R = data & 0xF;
			break;
		case 8:
			slot.RC = data >> 4;
			slot.RR = data & 0xF;
			break;
		case 9:
			slot.AM = data & 0x7;
			break;
		}
	} else {
		// All non-slot registers
		switch (reg) {
		case 0x00: // TEST
		case 0x01:
			break;

		case 0x02:
			// wave-table-header / memory-type / memory-access-mode
			// Simply store in regs[2]
			// Immediately write reg here and update memory pointers
			regs[2] = data;
			setupMemoryPointers();
			break;

		case 0x03:
			// Verified on real YMF278:
			// * Don't update the 'memAdr' variable on writes to
			//   reg 3 and 4. Only store the value in the 'regs'
			//   array for later use.
			// * The upper 2 bits are not used to address the
			//   external memories (so from a HW pov they don't
			//   matter). But if you read back this register, the
			//   upper 2 bits always read as '0' (even if you wrote
			//   '1'). So we mask the bits here already.
			data &= 0x3F;
			break;

		case 0x04:
			// See reg 3.
			break;

		case 0x05:
			// Verified on real YMF278: (see above)
			// Only writes to reg 5 change the (full) 'memAdr'.
			memAdr = (regs[3] << 16) | (regs[4] << 8) | data;
			break;

		case 0x06:  // memory data
			if (regs[2] & 1) {
				writeMem(memAdr, data);
				++memAdr; // no need to mask (again) here
			} else {
				// Verified on real YMF278:
				//  - writes are ignored
				//  - memAdr is NOT increased
			}
			break;

		case 0xf8: // These are implemented in MSXMoonSound.cc
		case 0xf9:
			break;
		}
	}

	regs[reg] = data;
}

uint8_t YMF278::readReg(uint8_t reg)
{
	// no need to call updateStream(time)
	uint8_t result = peekReg(reg);
	if (reg == 6) {
		// Memory Data Register
		if (regs[2] & 1) {
			// Verified on real YMF278:
			// memAdr is only increased when 'regs[2] & 1'
			++memAdr; // no need to mask (again) here
		}
	}
	return result;
}

uint8_t YMF278::peekReg(uint8_t reg) const
{
	switch (reg) {
		case 2: // 3 upper bits are device ID
			return (regs[2] & 0x1F) | 0x20;

		case 6: // Memory Data Register
			if (regs[2] & 1) {
				return readMem(memAdr);
			} else {
				// Verified on real YMF278
				return 0xff;
			}

		default:
			return regs[reg];
	}
}

static constexpr unsigned INPUT_RATE = 44100;

YMF278::YMF278(const std::string& name_, size_t ramSize, const DeviceConfig& config,
               SetupMemPtrFunc setupMemPtrs_)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "OPL4 wave-part",
	                       24, INPUT_RATE, true)
	, motherBoard(config.getMotherBoard())
	, debugRegisters(motherBoard, getName())
	, debugMemory   (motherBoard, getName())
	, rom(getName() + " ROM", "rom", config)
	, ram(config, getName() + " RAM", "YMF278 sample RAM", ramSize)
	, setupMemPtrs(setupMemPtrs_)
{
	if (rom.size() != 0x200000) { // 2MB
		throw MSXException(
			"Wrong ROM for OPL4 (YMF278B). The ROM (usually "
			"called yrw801.rom) should have a size of exactly 2MB.");
	}

	memAdr = 0; // avoid UMR
	std::ranges::fill(regs, 0);

	registerSound(config);
	reset(motherBoard.getCurrentTime()); // must come after registerSound() because of call to setSoftwareVolume() via setMixLevel()
}

YMF278::~YMF278()
{
	unregisterSound();
}

void YMF278::clearRam()
{
	ram.clear(0);
}

void YMF278::reset(EmuTime::param time)
{
	updateStream(time);

	eg_cnt = 0;

	for (auto& op : slots) {
		op.reset();
	}
	regs[2] = 0; // avoid UMR
	for (int i = 0xf7; i >= 0; --i) { // reverse order to avoid UMR
		writeRegDirect(narrow<uint8_t>(i), 0, time);
	}
	regs[0xf8] = 0x1b; // FM mix-level, see also YMF262 reset
	regs[0xf9] = 0x00; // Wave mix-level
	memAdr = 0;
	setMixLevel(0, time);

	setupMemoryPointers();
}

// The YMF278B chip has 10 chip-select output signals named /MCS0 .. /MSC9.
// There is a 4MB address space to store samples (e.g. in ROM or RAM). Depending on
// which address is accessed, one or more of these /MCSx signals get active. And there
// are two modes for this mapping (controlled via bit 1 in R#2):
//
//              mode=0              mode=1
//  /MCS0   0x000000-0x1FFFFF   0x000000-0x1FFFFF
//  /MCS1   0x200000-0x3FFFFF   0x000000-0x0FFFFF
//  /MCS2   0x000000-0x0FFFFF   0x100000-0x1FFFFF
//  /MCS3   0x100000-0x1FFFFF   0x200000-0x2FFFFF
//  /MCS4   0x200000-0x2FFFFF   0x200000-0x27FFFF
//  /MCS5   0x300000-0x3FFFFF   0x280000-0x2FFFFF
//  /MCS6   0x200000-0x27FFFF   0x380000-0x39FFFF
//  /MCS7   0x280000-0x2FFFFF   0x3A0000-0x3BFFFF
//  /MCS8   0x300000-0x37FFFF   0x3C0000-0x3DFFFF
//  /MCS9   0x380000-0x3FFFFF   0x3E0000-0x3FFFFF
void YMF278::setupMemoryPointers()
{
	bool mode0 = (regs[2] & 2) == 0;
	setupMemPtrs(mode0, rom, ram, memPtrs);
}

uint8_t YMF278::readMem(unsigned address) const
{
	// Verified on real YMF278: address space wraps at 4MB.
	address &= 0x3F'FFFF;
	if (auto chunk = memPtrs[address >> 17].asOptional()) { // 128kB chunk
		return (*chunk)[address & 0x1'FFFF];
	}
	return 0xFF;
}

void YMF278::writeMem(unsigned address, uint8_t value)
{
	address &= 0x3F'FFFF;
	if (auto chunk = memPtrs[address >> 17].asOptional()) { // mapped?
		auto* ptr = chunk->data() + (address & 0x1'ffff);
		if ((&ram[0] <= ptr) && (ptr < (&ram[0] + ram.size()))) { // points to RAM?
			// this assumes all RAM is emulated via a single contiguous memory block
			auto ramOffset = ptr - &ram[0];
			ram.write(ramOffset, value);
		}
	}
	// ignore writes to non-mapped, or non-RAM regions
}

// version 1: initial version, some variables were saved as char
// version 2: serialization framework was fixed to save/load chars as numbers
//            but for backwards compatibility we still load old savestates as
//            characters
// version 3: 'step' is no longer stored (it is recalculated)
// version 4:
//  - removed members: 'lfo', 'LD', 'active'
//  - new members 'TLdest', 'keyon', 'DAMP' restored from registers instead of serialized
//  - store 'OCT' sign-extended
//  - store 'endAddr' as 2s complement
//  - removed EG_DMP and EG_REV enum values from 'state'
// version 5:
//  - re-added 'lfo' member. This is not stored in the savestate, instead it's
//    restored from register values in YMF278::serialize()
//  - removed members 'lfo_step' and ' 'lfo_max'
//  - 'lfo_cnt' has changed meaning (but we don't try to translate old to new meaning)
// version 6:
//  - removed members: 'sample1', 'sample2'
template<typename Archive>
void YMF278::Slot::serialize(Archive& ar, unsigned version)
{
	// TODO restore more state from registers
	ar.serialize("startaddr", startAddr,
	             "loopaddr",  loopAddr,
	             "stepptr",   stepPtr,
	             "pos",       pos,
	             "env_vol",   env_vol,
	             "lfo_cnt",   lfo_cnt,
	             "DL",        DL,
	             "wave",      wave,
	             "FN",        FN);
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("endaddr", endAddr,
		             "OCT",     OCT);
	} else {
		unsigned e = 0; ar.serialize("endaddr", e); endAddr = uint16_t((e ^ 0xffff) + 1);

		char O = 0;
		if (ar.versionAtLeast(version, 2)) {
			ar.serialize("OCT", O);
		} else {
			ar.serializeChar("OCT", O);
		}
		OCT = sign_extend_4(O);
	}

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("PRVB", PRVB,
		             "TL",   TL,
		             "pan",  pan,
		             "vib",  vib,
		             "AM",   AM,
		             "AR",   AR,
		             "D1R",  D1R,
		             "D2R",  D2R,
		             "RC",   RC,
		             "RR",   RR);
	} else {
		// for backwards compatibility with old savestates
		char PRVB_ = 0; ar.serializeChar("PRVB", PRVB_); PRVB = PRVB_;
		char TL_  = 0; ar.serializeChar("TL",  TL_ ); TL  = TL_;
		char pan_ = 0; ar.serializeChar("pan", pan_); pan = pan_;
		char vib_ = 0; ar.serializeChar("vib", vib_); vib = vib_;
		char AM_  = 0; ar.serializeChar("AM",  AM_ ); AM  = AM_;
		char AR_  = 0; ar.serializeChar("AR",  AR_ ); AR  = AR_;
		char D1R_ = 0; ar.serializeChar("D1R", D1R_); D1R = D1R_;
		char D2R_ = 0; ar.serializeChar("D2R", D2R_); D2R = D2R_;
		char RC_  = 0; ar.serializeChar("RC",  RC_ ); RC  = RC_;
		char RR_  = 0; ar.serializeChar("RR",  RR_ ); RR  = RR_;
	}
	ar.serialize("bits",       bits,
	             "lfo_active", lfo_active);

	ar.serialize("state", state);
	if (ar.versionBelow(version, 4)) {
		assert(Archive::IS_LOADER);
		if (state == one_of(EG_REV, EG_DMP)) {
			state = EG_REL;
		}
	}

	// Recalculate redundant state
	if constexpr (Archive::IS_LOADER) {
		step = calcStep(OCT, FN);
	}

	// This old comment is NOT completely true:
	//    Older version also had "env_vol_step" and "env_vol_lim" but those
	//    members were nowhere used, so removed those in the current
	//    version (it's ok to remove members from the savestate without
	//    updating the version number).
	// When you remove member variables without increasing the version
	// number, new openMSX executables can still read old savestates. And
	// if you try to load a new savestate in an old openMSX version you do
	// get a (cryptic) error message. But if the version number is
	// increased the error message is much clearer.
}

// version 1: initial version
// version 2: loadTime and busyTime moved to MSXMoonSound class
// version 3: memAdr cannot be restored from register values
// version 4: implement ram via Ram class
template<typename Archive>
void YMF278::serialize(Archive& ar, unsigned version)
{
	ar.serialize("slots",  slots,
	             "eg_cnt", eg_cnt);
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("ram", ram);
	} else {
		ar.serialize_blob("ram", ram.getWriteBackdoor());
	}
	ar.serialize_blob("registers", regs);
	if (ar.versionAtLeast(version, 3)) { // must come after 'regs'
		ar.serialize("memadr", memAdr);
	} else {
		assert(Archive::IS_LOADER);
		// Old formats didn't store 'memAdr' so we also can't magically
		// restore the correct value. The best we can do is restore the
		// last set address.
		regs[3] &= 0x3F; // mask upper two bits
		memAdr = (regs[3] << 16) | (regs[4] << 8) | regs[5];
	}

	// TODO restore more state from registers
	if constexpr (Archive::IS_LOADER) {
		for (auto [i, sl] : enumerate(slots)) {
			uint8_t t = regs[0x50 + i] >> 1;
			sl.TLdest = (t != 0x7f) ? t : 0xff;

			sl.keyon = (regs[0x68 + i] & 0x80) != 0;
			sl.DAMP  = (regs[0x68 + i] & 0x40) != 0;
			sl.lfo   = (regs[0x80 + i] >> 3) & 7;
		}
	}
	// subclasses are responsible for calling setupMemoryPointers()
}
INSTANTIATE_SERIALIZE_METHODS(YMF278);


// class DebugRegisters

YMF278::DebugRegisters::DebugRegisters(MSXMotherBoard& motherBoard_,
                                       const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs",
	                   "OPL4 registers", 0x100)
{
}

uint8_t YMF278::DebugRegisters::read(unsigned address)
{
	const auto& ymf278 = OUTER(YMF278, debugRegisters);
	return ymf278.peekReg(narrow<uint8_t>(address));
}

void YMF278::DebugRegisters::write(unsigned address, uint8_t value, EmuTime::param time)
{
	auto& ymf278 = OUTER(YMF278, debugRegisters);
	ymf278.writeReg(narrow<uint8_t>(address), value, time);
}


// class DebugMemory

YMF278::DebugMemory::DebugMemory(MSXMotherBoard& motherBoard_,
                                 const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " mem",
	                   "OPL4 memory (includes both ROM and RAM)", 0x400000) // 4MB
{
}

uint8_t YMF278::DebugMemory::read(unsigned address)
{
	const auto& ymf278 = OUTER(YMF278, debugMemory);
	return ymf278.readMem(address);
}

void YMF278::DebugMemory::write(unsigned address, uint8_t value)
{
	auto& ymf278 = OUTER(YMF278, debugMemory);
	ymf278.writeMem(address, value);
}

} // namespace openmsx
