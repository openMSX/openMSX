/*
 *
 * File: ymf262.c - software implementation of YMF262
 *                  FM sound generator type OPL3
 *
 * Copyright (C) 2003 Jarek Burczynski
 *
 * Version 0.2
 *
 *
 * Revision History:
 *
 * 03-03-2003: initial release
 *  - thanks to Olivier Galibert and Chris Hardy for YMF262 and YAC512 chips
 *  - thanks to Stiletto for the datasheets
 *
 *
 *
 * differences between OPL2 and OPL3 not documented in Yamaha datasheets:
 * - sinus table is a little different: the negative part is off by one...
 *
 * - in order to enable selection of four different waveforms on OPL2
 *   one must set bit 5 in register 0x01(test).
 *   on OPL3 this bit is ignored and 4-waveform select works *always*.
 *   (Don't confuse this with OPL3's 8-waveform select.)
 *
 * - Envelope Generator: all 15 x rates take zero time on OPL3
 *   (on OPL2 15 0 and 15 1 rates take some time while 15 2 and 15 3 rates
 *   take zero time)
 *
 * - channel calculations: output of operator 1 is in perfect sync with
 *   output of operator 2 on OPL3; on OPL and OPL2 output of operator 1
 *   is always delayed by one sample compared to output of operator 2
 *
 *
 * differences between OPL2 and OPL3 shown in datasheets:
 * - YMF262 does not support CSM mode
 */

#include "YMF262.hh"

#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "Math.hh"
#include "cstd.hh"
#include "narrow.hh"
#include "outer.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace openmsx {

[[nodiscard]] static constexpr YMF262::FreqIndex fnumToIncrement(unsigned block_fnum)
{
	// opn phase increment counter = 20bit
	// chip works with 10.10 fixed point, while we use 16.16
	int block = narrow<int>((block_fnum & 0x1C00) >> 10);
	return YMF262::FreqIndex(block_fnum & 0x03FF) >> (11 - block);
}

// envelope output entries
static constexpr int ENV_BITS    = 10;
static constexpr int ENV_LEN     = 1 << ENV_BITS;
static constexpr double ENV_STEP = 128.0 / ENV_LEN;

static constexpr int MAX_ATT_INDEX = (1 << (ENV_BITS - 1)) - 1; // 511
static constexpr int MIN_ATT_INDEX = 0;

static constexpr int TL_RES_LEN = 256; // 8 bits addressing (real chip)

// register number to channel number , slot offset
static constexpr uint8_t MOD = 0;
static constexpr uint8_t CAR = 1;


// mapping of register number (offset) to slot number used by the emulator
static constexpr std::array<int, 32> slot_array = {
	 0,  2,  4,  1,  3,  5, -1, -1,
	 6,  8, 10,  7,  9, 11, -1, -1,
	12, 14, 16, 13, 15, 17, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};


// key scale level
// table is 3dB/octave , DV converts this into 6dB/octave
// 0.1875 is bit 0 weight of the envelope counter (volume) expressed
// in the 'decibel' scale
[[nodiscard]] static constexpr int DV(double x) { return int(x / (0.1875 / 2.0)); }
static constexpr std::array<unsigned, 8 * 16> ksl_tab = {
	// OCT 0
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	// OCT 1
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	DV( 0.000), DV( 0.750), DV( 1.125), DV( 1.500),
	DV( 1.875), DV( 2.250), DV( 2.625), DV( 3.000),
	// OCT 2
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 0.000),
	DV( 0.000), DV( 1.125), DV( 1.875), DV( 2.625),
	DV( 3.000), DV( 3.750), DV( 4.125), DV( 4.500),
	DV( 4.875), DV( 5.250), DV( 5.625), DV( 6.000),
	// OCT 3
	DV( 0.000), DV( 0.000), DV( 0.000), DV( 1.875),
	DV( 3.000), DV( 4.125), DV( 4.875), DV( 5.625),
	DV( 6.000), DV( 6.750), DV( 7.125), DV( 7.500),
	DV( 7.875), DV( 8.250), DV( 8.625), DV( 9.000),
	// OCT 4
	DV( 0.000), DV( 0.000), DV( 3.000), DV( 4.875),
	DV( 6.000), DV( 7.125), DV( 7.875), DV( 8.625),
	DV( 9.000), DV( 9.750), DV(10.125), DV(10.500),
	DV(10.875), DV(11.250), DV(11.625), DV(12.000),
	// OCT 5
	DV( 0.000), DV( 3.000), DV( 6.000), DV( 7.875),
	DV( 9.000), DV(10.125), DV(10.875), DV(11.625),
	DV(12.000), DV(12.750), DV(13.125), DV(13.500),
	DV(13.875), DV(14.250), DV(14.625), DV(15.000),
	// OCT 6
	DV( 0.000), DV( 6.000), DV( 9.000), DV(10.875),
	DV(12.000), DV(13.125), DV(13.875), DV(14.625),
	DV(15.000), DV(15.750), DV(16.125), DV(16.500),
	DV(16.875), DV(17.250), DV(17.625), DV(18.000),
	// OCT 7
	DV( 0.000), DV( 9.000), DV(12.000), DV(13.875),
	DV(15.000), DV(16.125), DV(16.875), DV(17.625),
	DV(18.000), DV(18.750), DV(19.125), DV(19.500),
	DV(19.875), DV(20.250), DV(20.625), DV(21.000)
};

// sustain level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
[[nodiscard]] static constexpr int SC(int db) { return int(db * (2.0 / ENV_STEP)); }
static constexpr std::array<int, 16> sl_tab = {
	SC( 0), SC( 1), SC( 2), SC(3 ), SC(4 ), SC(5 ), SC(6 ), SC( 7),
	SC( 8), SC( 9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
};


static constexpr uint8_t RATE_STEPS = 8;
static constexpr std::array<uint8_t, 15 * RATE_STEPS> eg_inc = {
//cycle:0 1  2 3  4 5  6 7
	0,1, 0,1, 0,1, 0,1, //  0  rates 00..12 0 (increment by 0 or 1)
	0,1, 0,1, 1,1, 0,1, //  1  rates 00..12 1
	0,1, 1,1, 0,1, 1,1, //  2  rates 00..12 2
	0,1, 1,1, 1,1, 1,1, //  3  rates 00..12 3

	1,1, 1,1, 1,1, 1,1, //  4  rate 13 0 (increment by 1)
	1,1, 1,2, 1,1, 1,2, //  5  rate 13 1
	1,2, 1,2, 1,2, 1,2, //  6  rate 13 2
	1,2, 2,2, 1,2, 2,2, //  7  rate 13 3

	2,2, 2,2, 2,2, 2,2, //  8  rate 14 0 (increment by 2)
	2,2, 2,4, 2,2, 2,4, //  9  rate 14 1
	2,4, 2,4, 2,4, 2,4, // 10  rate 14 2
	2,4, 4,4, 2,4, 4,4, // 11  rate 14 3

	4,4, 4,4, 4,4, 4,4, // 12  rates 15 0, 15 1, 15 2, 15 3 for decay
	8,8, 8,8, 8,8, 8,8, // 13  rates 15 0, 15 1, 15 2, 15 3 for attack (zero time)
	0,0, 0,0, 0,0, 0,0, // 14  infinity rates for attack and decay(s)
};


// note that there is no O(13) in this table - it's directly in the code
[[nodiscard]] static constexpr uint8_t O(int a) { return narrow<uint8_t>(a * RATE_STEPS); }
static constexpr std::array<uint8_t, 16 + 64 + 16> eg_rate_select = {
	// Envelope Generator rates (16 + 64 rates + 16 RKS)
	// 16 infinite time rates
	O(14), O(14), O(14), O(14), O(14), O(14), O(14), O(14),
	O(14), O(14), O(14), O(14), O(14), O(14), O(14), O(14),

	// rates 00-12
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),
	O( 0), O( 1), O( 2), O( 3),

	// rate 13
	O( 4), O( 5), O( 6), O( 7),

	// rate 14
	O( 8), O( 9), O(10), O(11),

	// rate 15
	O(12), O(12), O(12), O(12),

	// 16 dummy rates (same as 15 3)
	O(12), O(12), O(12), O(12), O(12), O(12), O(12), O(12),
	O(12), O(12), O(12), O(12), O(12), O(12), O(12), O(12),
};

// rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
// shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
// mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0
static constexpr std::array<uint8_t, 16 + 64 + 16> eg_rate_shift =
{
	// Envelope Generator counter shifts (16 + 64 rates + 16 RKS)
	// 16 infinite time rates
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,

	// rates 00-15
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

	// 16 dummy rates (same as 15 3)
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
};


// multiple table
[[nodiscard]] static constexpr uint8_t ML(double x) { return uint8_t(2 * x); }
static constexpr std::array<uint8_t, 16> mul_tab = {
	// 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,10,12,12,15,15
	ML( 0.5), ML( 1.0), ML( 2.0), ML( 3.0),
	ML( 4.0), ML( 5.0), ML( 6.0), ML( 7.0),
	ML( 8.0), ML( 9.0), ML(10.0), ML(10.0),
	ML(12.0), ML(12.0), ML(15.0), ML(15.0)
};

// LFO Amplitude Modulation table (verified on real YM3812)
//  27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples
//
// Length: 210 elements
//
// Each of the elements has to be repeated
// exactly 64 times (on 64 consecutive samples).
// The whole table takes: 64 * 210 = 13440 samples.
//
// When AM = 1 data is used directly
// When AM = 0 data is divided by 4 before being used (loosing precision is important)

static constexpr unsigned LFO_AM_TAB_ELEMENTS = 210;
static constexpr std::array<uint8_t, LFO_AM_TAB_ELEMENTS> lfo_am_table = {
	 0,  0,  0, /**/
	 0,  0,  0,  0,
	 1,  1,  1,  1,
	 2,  2,  2,  2,
	 3,  3,  3,  3,
	 4,  4,  4,  4,
	 5,  5,  5,  5,
	 6,  6,  6,  6,
	 7,  7,  7,  7,
	 8,  8,  8,  8,
	 9,  9,  9,  9,
	10, 10, 10, 10,
	11, 11, 11, 11,
	12, 12, 12, 12,
	13, 13, 13, 13,
	14, 14, 14, 14,
	15, 15, 15, 15,
	16, 16, 16, 16,
	17, 17, 17, 17,
	18, 18, 18, 18,
	19, 19, 19, 19,
	20, 20, 20, 20,
	21, 21, 21, 21,
	22, 22, 22, 22,
	23, 23, 23, 23,
	24, 24, 24, 24,
	25, 25, 25, 25,
	26, 26, 26, /**/
	25, 25, 25, 25,
	24, 24, 24, 24,
	23, 23, 23, 23,
	22, 22, 22, 22,
	21, 21, 21, 21,
	20, 20, 20, 20,
	19, 19, 19, 19,
	18, 18, 18, 18,
	17, 17, 17, 17,
	16, 16, 16, 16,
	15, 15, 15, 15,
	14, 14, 14, 14,
	13, 13, 13, 13,
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
	 1,  1,  1,  1
};

// LFO Phase Modulation table (verified on real YM3812)
static constexpr std::array<int8_t, 8 * 8 * 2> lfo_pm_table = {
	// FNUM2/FNUM = 00 0xxxxxxx (0x0000)
	0, 0, 0, 0, 0, 0, 0, 0, // LFO PM depth = 0
	0, 0, 0, 0, 0, 0, 0, 0, // LFO PM depth = 1

	// FNUM2/FNUM = 00 1xxxxxxx (0x0080)
	0, 0, 0, 0, 0, 0, 0, 0, // LFO PM depth = 0
	1, 0, 0, 0,-1, 0, 0, 0, // LFO PM depth = 1

	// FNUM2/FNUM = 01 0xxxxxxx (0x0100)
	1, 0, 0, 0,-1, 0, 0, 0, // LFO PM depth = 0
	2, 1, 0,-1,-2,-1, 0, 1, // LFO PM depth = 1

	// FNUM2/FNUM = 01 1xxxxxxx (0x0180)
	1, 0, 0, 0,-1, 0, 0, 0, // LFO PM depth = 0
	3, 1, 0,-1,-3,-1, 0, 1, // LFO PM depth = 1

	// FNUM2/FNUM = 10 0xxxxxxx (0x0200)
	2, 1, 0,-1,-2,-1, 0, 1, // LFO PM depth = 0
	4, 2, 0,-2,-4,-2, 0, 2, // LFO PM depth = 1

	// FNUM2/FNUM = 10 1xxxxxxx (0x0280)
	2, 1, 0,-1,-2,-1, 0, 1, // LFO PM depth = 0
	5, 2, 0,-2,-5,-2, 0, 2, // LFO PM depth = 1

	// FNUM2/FNUM = 11 0xxxxxxx (0x0300)
	3, 1, 0,-1,-3,-1, 0, 1, // LFO PM depth = 0
	6, 3, 0,-3,-6,-3, 0, 3, // LFO PM depth = 1

	// FNUM2/FNUM = 11 1xxxxxxx (0x0380)
	3, 1, 0,-1,-3,-1, 0, 1, // LFO PM depth = 0
	7, 3, 0,-3,-7,-3, 0, 3  // LFO PM depth = 1
};

// TL_TAB_LEN is calculated as:
//  (12+1)=13 - sinus amplitude bits     (Y axis)
//  additional 1: to compensate for calculations of negative part of waveform
//  (if we don't add it then the greatest possible _negative_ value would be -2
//  and we really need -1 for waveform #7)
//  2  - sinus sign bit           (Y axis)
//  TL_RES_LEN - sinus resolution (X axis)
static constexpr int TL_TAB_LEN = 13 * 2 * TL_RES_LEN;
static constexpr int ENV_QUIET = TL_TAB_LEN >> 4;

static constexpr auto tlTab = [] {
	std::array<int, TL_TAB_LEN> result = {};
	// this _is_ different from OPL2 (verified on real YMF262)
	for (auto x : xrange(TL_RES_LEN)) {
		double m = (1 << 16) / cstd::exp2<6>((x + 1) * (ENV_STEP / 4.0) / 8.0);

		// we never reach (1<<16) here due to the (x+1)
		// result fits within 16 bits at maximum
		auto n = int(m);        // 16 bits here
		n >>= 4;                // 12 bits here
		n = (n >> 1) + (n & 1); // round to nearest
		// 11 bits here (rounded)
		n <<= 1; // 12 bits here (as in real chip)
		result[x * 2 + 0] = n;
		result[x * 2 + 1] = ~result[x * 2 + 0];

		for (int i : xrange(1, 13)) {
			result[x * 2 + 0 + i * 2 * TL_RES_LEN] =
			        result[x * 2 + 0] >> i;
			result[x * 2 + 1 + i * 2 * TL_RES_LEN] =
			        ~result[x * 2 + 0 + i * 2 * TL_RES_LEN];
		}
	}
	return result;
}();


// sin waveform table in 'decibel' scale
// there are eight waveforms on OPL3 chips
struct SinTab {
	std::array<std::array<unsigned, YMF262::SIN_LEN>, 8> tab;
};

static constexpr SinTab getSinTab()
{
	SinTab sin = {};

	constexpr auto SIN_BITS = YMF262::SIN_BITS;
	constexpr auto SIN_LEN  = YMF262::SIN_LEN;
	constexpr auto SIN_MASK = YMF262::SIN_MASK;
	for (auto i : xrange(SIN_LEN / 4)) {
		// non-standard sinus
		double m = cstd::sin<2>(((i * 2) + 1) * Math::pi / SIN_LEN); // checked against the real chip
		// we never reach zero here due to ((i * 2) + 1)
		double o = -8.0 * cstd::log2<11, 3>(m); // convert to 'decibels'
		o = o / (double(ENV_STEP) / 4);

		auto n = int(2 * o);
		n = (n >> 1) + (n & 1); // round to nearest
		sin.tab[0][i] = 2 * n;
	}
	for (auto i : xrange(SIN_LEN / 4)) {
		sin.tab[0][SIN_LEN / 2 - 1 - i] = sin.tab[0][i];
	}
	for (auto i : xrange(SIN_LEN / 2)) {
		sin.tab[0][SIN_LEN / 2 + i] = sin.tab[0][i] + 1;
	}

	for (auto i : xrange(SIN_LEN)) {
		// these 'pictures' represent _two_ cycles
		// waveform 1:  __      __
		//             /  \____/  \____
		// output only first half of the sinus waveform (positive one)
		sin.tab[1][i] = (i & (1 << (SIN_BITS - 1)))
		              ? TL_TAB_LEN
		              : sin.tab[0][i];

		// waveform 2:  __  __  __  __
		//             /  \/  \/  \/  \.
		// abs(sin)
		sin.tab[2][i] = sin.tab[0][i & (SIN_MASK >> 1)];

		// waveform 3:  _   _   _   _
		//             / |_/ |_/ |_/ |_
		// abs(output only first quarter of the sinus waveform)
		sin.tab[3][i] = (i & (1 << (SIN_BITS - 2)))
		              ? TL_TAB_LEN
		              : sin.tab[0][i & (SIN_MASK>>2)];

		// waveform 4: /\  ____/\  ____
		//               \/      \/
		// output whole sinus waveform in half the cycle(step=2)
		// and output 0 on the other half of cycle
		sin.tab[4][i] = (i & (1 << (SIN_BITS - 1)))
		              ? TL_TAB_LEN
		              : sin.tab[0][i * 2];

		// waveform 5: /\/\____/\/\____
		//
		// output abs(whole sinus) waveform in half the cycle(step=2)
		// and output 0 on the other half of cycle
		sin.tab[5][i] = (i & (1 << (SIN_BITS - 1)))
		              ? TL_TAB_LEN
		              : sin.tab[0][(i * 2) & (SIN_MASK >> 1)];

		// waveform 6: ____    ____
		//                 ____    ____
		// output maximum in half the cycle and output minimum
		// on the other half of cycle
		sin.tab[6][i] = (i & (1 << (SIN_BITS - 1)))
		              ? 1  // negative
		              : 0; // positive

		// waveform 7:|\____  |\____
		//                   \|      \|
		// output sawtooth waveform
		int x = (i & (1 << (SIN_BITS - 1)))
		      ? ((SIN_LEN - 1) - i) * 16 + 1  // negative: from 8177 to 1
		      : i * 16;                       // positive: from 0 to 8176
		x = std::min(x, TL_TAB_LEN); // clip to the allowed range
		sin.tab[7][i] = x;
	}

	return sin;
}

static constexpr SinTab sin = getSinTab();


// TODO clean this up
static int phase_modulation;  // phase modulation input (SLOT 2)
static int phase_modulation2; // phase modulation input (SLOT 3
                              // in 4 operator channels)


YMF262::Slot::Slot()
	: waveTable(sin.tab[0])
{
}


void YMF262::callback(uint8_t flag)
{
	setStatus(flag);
}

// status set and IRQ handling
void YMF262::setStatus(uint8_t flag)
{
	// set status flag masking out disabled IRQs
	status |= flag;
	if (status & statusMask) {
		status |= 0x80;
		irq.set();
	}
}

// status reset and IRQ handling
void YMF262::resetStatus(uint8_t flag)
{
	// reset status flag
	status &= ~flag;
	if (!(status & statusMask)) {
		status &= 0x7F;
		irq.reset();
	}
}

// IRQ mask set
void YMF262::changeStatusMask(uint8_t flag)
{
	statusMask = flag;
	status &= statusMask;
	if (status) {
		status |= 0x80;
		irq.set();
	} else {
		status &= 0x7F;
		irq.reset();
	}
}

void YMF262::Slot::advanceEnvelopeGenerator(unsigned egCnt)
{
	switch (state) {
	using enum EnvelopeState;
	case ATTACK:
		if (!(egCnt & eg_m_ar)) {
			volume += (~volume * eg_inc[eg_sel_ar + ((egCnt >> eg_sh_ar) & 7)]) >> 3;
			if (volume <= MIN_ATT_INDEX) {
				volume = MIN_ATT_INDEX;
				state = DECAY;
			}
		}
		break;

	case DECAY:
		if (!(egCnt & eg_m_dr)) {
			volume += eg_inc[eg_sel_dr + ((egCnt >> eg_sh_dr) & 7)];
			if (volume >= sl) {
				state = SUSTAIN;
			}
		}
		break;

	case SUSTAIN:
		// this is important behaviour:
		// one can change percussive/non-percussive
		// modes on the fly and the chip will remain
		// in sustain phase - verified on real YM3812
		if (eg_type) {
			// non-percussive mode
			// do nothing
		} else {
			// percussive mode
			// during sustain phase chip adds Release Rate (in percussive mode)
			if (!(egCnt & eg_m_rr)) {
				volume += eg_inc[eg_sel_rr + ((egCnt >> eg_sh_rr) & 7)];
				volume = std::min(volume, MAX_ATT_INDEX);
			} else {
				// do nothing in sustain phase
			}
		}
		break;

	case RELEASE:
		if (!(egCnt & eg_m_rr)) {
			volume += eg_inc[eg_sel_rr + ((egCnt >> eg_sh_rr) & 7)];
			if (volume >= MAX_ATT_INDEX) {
				volume = MAX_ATT_INDEX;
				state = OFF;
			}
		}
		break;

	default:
		break;
	}
}

void YMF262::Slot::advancePhaseGenerator(const Channel& ch, unsigned lfo_pm)
{
	if (vib) {
		// LFO phase modulation active
		unsigned block_fnum = ch.block_fnum;
		unsigned fnum_lfo   = (block_fnum & 0x0380) >> 7;
		auto lfo_fn_table_index_offset = narrow_cast<int>(lfo_pm_table[lfo_pm + 16 * fnum_lfo]);
		Cnt += fnumToIncrement(block_fnum + lfo_fn_table_index_offset) * mul;
	} else {
		// LFO phase modulation disabled for this operator
		Cnt += Incr;
	}
}

// advance to next sample
void YMF262::advance()
{
	// Vibrato: 8 output levels (triangle waveform);
	// 1 level takes 1024 samples
	lfo_pm_cnt.addQuantum();
	unsigned lfo_pm = (lfo_pm_cnt.toInt() & 7) | lfo_pm_depth_range;

	++eg_cnt;
	for (auto i : xrange(channel.size())) {
		auto& ch = channel[i];
		auto& ch2 = isExtended(i) ? getFirstOfPair(i) : ch;
		for (auto& op : ch.slot) {
			op.advanceEnvelopeGenerator(eg_cnt);
			op.advancePhaseGenerator(ch2, lfo_pm);
		}
	}

	// The Noise Generator of the YM3812 is 23-bit shift register.
	// Period is equal to 2^23-2 samples.
	// Register works at sampling frequency of the chip, so output
	// can change on every sample.
	//
	// Output of the register and input to the bit 22 is:
	// bit0 XOR bit14 XOR bit15 XOR bit22
	//
	// Simply use bit 22 as the noise output.
	//
	// unsigned j = ((noise_rng >>  0) ^ (noise_rng >> 14) ^
	//               (noise_rng >> 15) ^ (noise_rng >> 22)) & 1;
	// noise_rng = (j << 22) | (noise_rng >> 1);
	//
	// Instead of doing all the logic operations above, we
	// use a trick here (and use bit 0 as the noise output).
	// The difference is only that the noise bit changes one
	// step ahead. This doesn't matter since we don't know
	// what is real state of the noise_rng after the reset.
	if (noise_rng & 1) {
		noise_rng ^= 0x800302;
	}
	noise_rng >>= 1;
}

inline int YMF262::Slot::op_calc(unsigned phase, unsigned lfo_am) const
{
	unsigned env = (TLL + volume + (lfo_am & AMmask)) << 4;
	auto p = env + waveTable[phase & SIN_MASK];
	return (p < TL_TAB_LEN) ? tlTab[p] : 0;
}

// calculate output of a standard 2 operator channel
// (or 1st part of a 4-op channel)
void YMF262::Channel::chan_calc(unsigned lfo_am)
{
	// !! something is wrong with this, it caused bug
	// !!    [2823673] MoonSound 4 operator FM fail
	// !! optimization disabled for now
	// !! TODO investigate
	// !!   maybe this micro optimization isn't worth the trouble/risk
	// !!
	// - mod.connect can point to 'phase_modulation'  or 'ch0-output'
	// - car.connect can point to 'phase_modulation2' or 'ch0-output'
	//    (see register #C0-#C8 writes)
	// - phase_modulation2 is only used in 4op mode
	// - mod.connect and car.connect can point to the same thing, so we need
	//   an addition for car.connect (and initialize phase_modulation2 to
	//   zero). For mod.connect we can directly assign the value.

	// ?? is this paragraph correct ??
	// phase_modulation should be initialized to zero here. But there seems
	// to be an optimization bug in gcc-4.2: it *seems* that when we
	// initialize phase_modulation to zero in this function, the optimizer
	// assumes it still has value zero at the end of this function (where
	// it's used to calculate car.connect). As a workaround we initialize
	// phase_modulation each time before calling this function.
	phase_modulation = 0;
	phase_modulation2 = 0;

	auto& mod = slot[MOD];
	int out = mod.fb_shift
		? mod.op1_out[0] + mod.op1_out[1]
		: 0;
	mod.op1_out[0] = mod.op1_out[1];
	mod.op1_out[1] = mod.op_calc(mod.Cnt.toInt() + (out >> mod.fb_shift), lfo_am);
	*mod.connect += mod.op1_out[1];

	auto& car = slot[CAR];
	*car.connect += car.op_calc(car.Cnt.toInt() + phase_modulation, lfo_am);
}

// calculate output of a 2nd part of 4-op channel
void YMF262::Channel::chan_calc_ext(unsigned lfo_am)
{
	// !! see remark in chan_cal(), something is wrong with this
	// !! optimization disabled for now
	// !!
	// - mod.connect can point to 'phase_modulation' or 'ch3-output'
	// - car.connect always points to 'ch3-output'  (always 4op-mode)
	//    (see register #C0-#C8 writes)
	// - mod.connect and car.connect can point to the same thing, so we need
	//   an addition for car.connect. For mod.connect we can directly assign
	//   the value.

	phase_modulation = 0;

	auto& mod = slot[MOD];
	*mod.connect += mod.op_calc(mod.Cnt.toInt() + phase_modulation2, lfo_am);

	auto& car = slot[CAR];
	*car.connect += car.op_calc(car.Cnt.toInt() + phase_modulation, lfo_am);
}

// operators used in the rhythm sounds generation process:
//
// Envelope Generator:
//
// channel  operator  register number   Bass  High  Snare Tom  Top
// / slot   number    TL ARDR SLRR Wave Drum  Hat   Drum  Tom  Cymbal
//  6 / 0   12        50  70   90   f0  +
//  6 / 1   15        53  73   93   f3  +
//  7 / 0   13        51  71   91   f1        +
//  7 / 1   16        54  74   94   f4              +
//  8 / 0   14        52  72   92   f2                    +
//  8 / 1   17        55  75   95   f5                          +
//
// Phase Generator:
//
// channel  operator  register number   Bass  High  Snare Tom  Top
// / slot   number    MULTIPLE          Drum  Hat   Drum  Tom  Cymbal
//  6 / 0   12        30                +
//  6 / 1   15        33                +
//  7 / 0   13        31                      +     +           +
//  7 / 1   16        34                -----  n o t  u s e d -----
//  8 / 0   14        32                                  +
//  8 / 1   17        35                      +                 +
//
// channel  operator  register number   Bass  High  Snare Tom  Top
// number   number    BLK/FNUM2 FNUM    Drum  Hat   Drum  Tom  Cymbal
//    6     12,15     B6        A6      +
//
//    7     13,16     B7        A7            +     +           +
//
//    8     14,17     B8        A8            +           +     +

// The following formulas can be well optimized.
// I leave them in direct form for now (in case I've missed something).

inline unsigned YMF262::genPhaseHighHat()
{
	// high hat phase generation (verified on real YM3812):
	// phase = d0 or 234 (based on frequency only)
	// phase = 34 or 2d0 (based on noise)

	// base frequency derived from operator 1 in channel 7
	int op71phase = channel[7].slot[MOD].Cnt.toInt();
	bool bit7 = (op71phase & 0x80) != 0;
	bool bit3 = (op71phase & 0x08) != 0;
	bool bit2 = (op71phase & 0x04) != 0;
	bool res1 = (bit2 ^ bit7) | bit3;
	// when res1 = 0 phase = 0x000 | 0xd0;
	// when res1 = 1 phase = 0x200 | (0xd0>>2);
	unsigned phase = res1 ? (0x200 | (0xd0 >> 2)) : 0xd0;

	// enable gate based on frequency of operator 2 in channel 8
	int op82phase = channel[8].slot[CAR].Cnt.toInt();
	bool bit5e= (op82phase & 0x20) != 0;
	bool bit3e= (op82phase & 0x08) != 0;
	bool res2 = (bit3e ^ bit5e);
	// when res2 = 0 pass the phase from calculation above (res1);
	// when res2 = 1 phase = 0x200 | (0xd0>>2);
	if (res2) {
		phase = (0x200 | (0xd0 >> 2));
	}

	// when phase & 0x200 is set and noise=1 then phase = 0x200|0xd0
	// when phase & 0x200 is set and noise=0 then phase = 0x200|(0xd0>>2), ie no change
	if (phase & 0x200) {
		if (noise_rng & 1) {
			phase = 0x200 | 0xd0;
		}
	} else {
	// when phase & 0x200 is clear and noise=1 then phase = 0xd0>>2
	// when phase & 0x200 is clear and noise=0 then phase = 0xd0, ie no change
		if (noise_rng & 1) {
			phase = 0xd0 >> 2;
		}
	}
	return phase;
}

inline unsigned YMF262::genPhaseSnare()
{
	// verified on real YM3812
	// base frequency derived from operator 1 in channel 7
	// noise bit XOR'es phase by 0x100
	return ((channel[7].slot[MOD].Cnt.toInt() & 0x100) + 0x100)
	     ^ ((noise_rng & 1) << 8);
}

inline unsigned YMF262::genPhaseCymbal()
{
	// verified on real YM3812
	// enable gate based on frequency of operator 2 in channel 8
	//  NOTE: YM2413_2 uses bit5 | bit3, this core uses bit5 ^ bit3
	//        most likely only one of the two is correct
	int op82phase = channel[8].slot[CAR].Cnt.toInt();
	if ((op82phase ^ (op82phase << 2)) & 0x20) { // bit5 ^ bit3
		return 0x300;
	} else {
		// base frequency derived from operator 1 in channel 7
		int op71phase = channel[7].slot[MOD].Cnt.toInt();
		bool bit7 = (op71phase & 0x80) != 0;
		bool bit3 = (op71phase & 0x08) != 0;
		bool bit2 = (op71phase & 0x04) != 0;
		return ((bit2 != bit7) || bit3) ? 0x300 : 0x100;
	}
}

// calculate rhythm
void YMF262::chan_calc_rhythm(unsigned lfo_am)
{
	// Bass Drum (verified on real YM3812):
	//  - depends on the channel 6 'connect' register:
	//      when connect = 0 it works the same as in normal (non-rhythm)
	//      mode (op1->op2->out)
	//      when connect = 1 _only_ operator 2 is present on output
	//      (op2->out), operator 1 is ignored
	//  - output sample always is multiplied by 2
	auto& mod6 = channel[6].slot[MOD];
	int out = mod6.fb_shift ? mod6.op1_out[0] + mod6.op1_out[1] : 0;
	mod6.op1_out[0] = mod6.op1_out[1];
	int pm = mod6.CON ? 0 : mod6.op1_out[0];
	mod6.op1_out[1] = mod6.op_calc(mod6.Cnt.toInt() + (out >> mod6.fb_shift), lfo_am);
	const auto& car6 = channel[6].slot[CAR];
	chanOut[6] += 2 * car6.op_calc(car6.Cnt.toInt() + pm, lfo_am);

	// Phase generation is based on:
	// HH  (13) channel 7->slot 1 combined with channel 8->slot 2
	//          (same combination as TOP CYMBAL but different output phases)
	// SD  (16) channel 7->slot 1
	// TOM (14) channel 8->slot 1
	// TOP (17) channel 7->slot 1 combined with channel 8->slot 2
	//          (same combination as HIGH HAT but different output phases)
	//
	// Envelope generation based on:
	// HH  channel 7->slot1
	// SD  channel 7->slot2
	// TOM channel 8->slot1
	// TOP channel 8->slot2
	const auto& mod7 = channel[7].slot[MOD];
	chanOut[7] += 2 * mod7.op_calc(genPhaseHighHat(), lfo_am);
	const auto& car7 = channel[7].slot[CAR];
	chanOut[7] += 2 * car7.op_calc(genPhaseSnare(),   lfo_am);
	const auto& mod8 = channel[8].slot[MOD];
	chanOut[8] += 2 * mod8.op_calc(mod8.Cnt.toInt(),  lfo_am);
	const auto& car8 = channel[8].slot[CAR];
	chanOut[8] += 2 * car8.op_calc(genPhaseCymbal(),  lfo_am);
}

void YMF262::Slot::FM_KEYON(uint8_t key_set)
{
	if (!key) {
		// restart Phase Generator
		Cnt = FreqIndex(0);
		// phase -> Attack
		state = EnvelopeState::ATTACK;
	}
	key |= key_set;
}

void YMF262::Slot::FM_KEYOFF(uint8_t key_clr)
{
	if (key) {
		key &= ~key_clr;
		if (!key) {
			// phase -> Release
			if (state != EnvelopeState::OFF) {
				state = EnvelopeState::RELEASE;
			}
		}
	}
}

void YMF262::Slot::update_ar_dr()
{
	if ((ar + ksr) < 16 + 60) {
		// verified on real YMF262 - all 15 x rates take "zero" time
		eg_sh_ar  = eg_rate_shift [ar + ksr];
		eg_sel_ar = eg_rate_select[ar + ksr];
	} else {
		eg_sh_ar  = 0;
		eg_sel_ar = 13 * RATE_STEPS;
	}
	eg_m_ar   = (1 << eg_sh_ar) - 1;
	eg_sh_dr  = eg_rate_shift [dr + ksr];
	eg_sel_dr = eg_rate_select[dr + ksr];
	eg_m_dr   = (1 << eg_sh_dr) - 1;
}
void YMF262::Slot::update_rr()
{
	eg_sh_rr  = eg_rate_shift [rr + ksr];
	eg_sel_rr = eg_rate_select[rr + ksr];
	eg_m_rr   = (1 << eg_sh_rr) - 1;
}

// update phase increment counter of operator (also update the EG rates if necessary)
void YMF262::Slot::calc_fc(const Channel& ch)
{
	// (frequency) phase increment counter
	Incr = ch.fc * mul;

	auto newKsr = narrow<uint8_t>(ch.kcode >> KSR);
	if (ksr == newKsr) return;
	ksr = newKsr;

	// calculate envelope generator rates
	update_ar_dr();
	update_rr();
}

static constexpr std::array<unsigned, 18> channelPairTab = {
	0,  1,  2,  0,  1,  2, unsigned(~0), unsigned(~0), unsigned(~0),
	9, 10, 11,  9, 10, 11, unsigned(~0), unsigned(~0), unsigned(~0),
};
inline bool YMF262::isExtended(unsigned ch) const
{
	assert(ch < 18);
	if (!OPL3_mode) return false;
	if (channelPairTab[ch] == unsigned(~0)) return false;
	return channel[channelPairTab[ch]].extended;
}
static constexpr unsigned getFirstOfPairNum(unsigned ch)
{
	assert((ch < 18) && (channelPairTab[ch] != unsigned(~0)));
	return channelPairTab[ch];
}
inline YMF262::Channel& YMF262::getFirstOfPair(unsigned ch)
{
	return channel[getFirstOfPairNum(ch) + 0];
}
inline YMF262::Channel& YMF262::getSecondOfPair(unsigned ch)
{
	return channel[getFirstOfPairNum(ch) + 3];
}

// set multi,am,vib,EG-TYP,KSR,mul
void YMF262::set_mul(unsigned sl, uint8_t v)
{
	unsigned chan_no = sl / 2;
	auto& ch = channel[chan_no];
	auto& slot = ch.slot[sl & 1];

	slot.mul     = mul_tab[v & 0x0f];
	slot.KSR     = (v & 0x10) ? 0 : 2;
	slot.eg_type = (v & 0x20) != 0;
	slot.vib     = (v & 0x40) != 0;
	slot.AMmask  = (v & 0x80) ? ~0 : 0;

	if (isExtended(chan_no)) {
		// 4op mode
		// update this slot using frequency data for 1st channel of a pair
		slot.calc_fc(getFirstOfPair(chan_no));
	} else {
		// normal (OPL2 mode or 2op mode)
		slot.calc_fc(ch);
	}
}

// set ksl & tl
void YMF262::set_ksl_tl(unsigned sl, uint8_t v)
{
	unsigned chan_no = sl / 2;
	auto& ch = channel[chan_no];
	auto& slot = ch.slot[sl & 1];

	// This is indeed {0.0, 3.0, 1.5, 6.0} dB/oct, verified on real YMF262.
	// Note the illogical order of 2nd and 3rd element.
	static constexpr std::array<uint8_t, 4> ksl_shift = {31, 1, 2, 0};
	slot.ksl = ksl_shift[v >> 6];

	slot.TL  = (v & 0x3F) << (ENV_BITS - 1 - 7); // 7 bits TL (bit 6 = always 0)

	if (isExtended(chan_no)) {
		// update this slot using frequency data for 1st channel of a pair
		const auto& ch0 = getFirstOfPair(chan_no);
		slot.TLL = slot.TL + (ch0.ksl_base >> slot.ksl);
	} else {
		// normal
		slot.TLL = slot.TL + (ch.ksl_base >> slot.ksl);
	}
}

// set attack rate & decay rate
void YMF262::set_ar_dr(unsigned sl, uint8_t v)
{
	auto& ch = channel[sl / 2];
	auto& slot = ch.slot[sl & 1];

	slot.ar = (v >> 4)   ? narrow<uint8_t>(16 + ((v >> 4)   << 2)) : 0;
	slot.dr = (v & 0x0F) ? narrow<uint8_t>(16 + ((v & 0x0F) << 2)) : 0;
	slot.update_ar_dr();
}

// set sustain level & release rate
void YMF262::set_sl_rr(unsigned sl, uint8_t v)
{
	auto& ch = channel[sl / 2];
	auto& slot = ch.slot[sl & 1];

	slot.sl  = sl_tab[v >> 4];
	slot.rr  = (v & 0x0F) ? uint8_t(16 + ((v & 0x0F) << 2)) : 0;
	slot.update_rr();
}

uint8_t YMF262::readReg(unsigned r) const
{
	// no need to call updateStream(time)
	return peekReg(r);
}

uint8_t YMF262::peekReg(unsigned r) const
{
	return reg[r];
}

void YMF262::writeReg(unsigned r, uint8_t v, EmuTime time)
{
	if (!OPL3_mode && (r != 0x105)) {
		// in OPL2 mode the only accessible in set #2 is register 0x05
		r &= ~0x100;
	}
	writeReg512(r, v, time);
}
void YMF262::writeReg512(unsigned r, uint8_t v, EmuTime time)
{
	updateStream(time); // TODO optimize only for regs that directly influence sound
	writeRegDirect(r, v, time);
}
void YMF262::writeRegDirect(unsigned r, uint8_t v, EmuTime time)
{
	reg[r] = v;

	unsigned ch_offset = (r & 0x100) ? 9 : 0;
	switch (r & 0xE0) {
	case 0x00: // 000-01F,100-11F: control
		switch (r) {
		case 0x000: // test register
		case 0x001: // test register
			break;

		case 0x002: // Timer 1
			timer1->setValue(v);
			break;

		case 0x003: // Timer 2
			timer2->setValue(v);
			break;

		case 0x004: // IRQ clear / mask and Timer enable
			if (v & 0x80) {
				// IRQ flags clear
				resetStatus(0x60);
			} else {
				changeStatusMask((~v) & 0x60);
				timer1->setStart((v & R04_ST1) != 0, time);
				timer2->setStart((v & R04_ST2) != 0, time);
			}
			break;

		case 0x008: // x,NTS,x,x, x,x,x,x
			nts = (v & 0x40) != 0;
			break;

		case 0x100: // test register
		case 0x101: // test register
			break;

		case 0x104:
			// 6 channels enable
			channel[ 0].extended = (v & 0x01) != 0;
			channel[ 1].extended = (v & 0x02) != 0;
			channel[ 2].extended = (v & 0x04) != 0;
			channel[ 9].extended = (v & 0x08) != 0;
			channel[10].extended = (v & 0x10) != 0;
			channel[11].extended = (v & 0x20) != 0;
			return;

		case 0x105:
			// OPL3 mode when bit0=1 otherwise it is OPL2 mode
			OPL3_mode = v & 0x01;

			// Verified on real YMF278: When NEW2 bit is first set, a read
			// from the status register (once) returns bit 1 set (0x02).
			// This only happens once after reset, so clearing NEW2 and
			// setting it again doesn't cause another change in the status
			// register. Also, only bit 1 changes.
			if ((v & 0x02) && !alreadySignaledNEW2 && isYMF278) {
				status2 = 0x02;
				alreadySignaledNEW2 = true;
			}

			// following behaviour was tested on real YMF262,
			// switching OPL3/OPL2 modes on the fly:
			//  - does not change the waveform previously selected
			//    (unless when ....)
			//  - does not update CH.A, CH.B, CH.C and CH.D output
			//    selectors (registers c0-c8) (unless when ....)
			//  - does not disable channels 9-17 on OPL3->OPL2 switch
			//  - does not switch 4 operator channels back to 2
			//    operator channels
			return;

		default:
			break;
		}
		break;

	case 0x20: { // am ON, vib ON, ksr, eg_type, mul
		int slot = slot_array[r & 0x1F];
		if (slot < 0) return;
		set_mul(slot + ch_offset * 2, v);
		break;
	}
	case 0x40: {
		int slot = slot_array[r & 0x1F];
		if (slot < 0) return;
		set_ksl_tl(slot + ch_offset * 2, v);
		break;
	}
	case 0x60: {
		int slot = slot_array[r & 0x1F];
		if (slot < 0) return;
		set_ar_dr(slot + ch_offset * 2, v);
		break;
	}
	case 0x80: {
		int slot = slot_array[r & 0x1F];
		if (slot < 0) return;
		set_sl_rr(slot + ch_offset * 2, v);
		break;
	}
	case 0xA0: {
		// note: not r != 0x1BD, only first register block
		if (r == 0xBD) {
			// am depth, vibrato depth, r,bd,sd,tom,tc,hh
			lfo_am_depth = (v & 0x80) != 0;
			lfo_pm_depth_range = (v & 0x40) ? 8 : 0;
			rhythm = v & 0x3F;

			if (rhythm & 0x20) {
				// BD key on/off
				if (v & 0x10) {
					channel[6].slot[MOD].FM_KEYON (2);
					channel[6].slot[CAR].FM_KEYON (2);
				} else {
					channel[6].slot[MOD].FM_KEYOFF(2);
					channel[6].slot[CAR].FM_KEYOFF(2);
				}
				// HH key on/off
				if (v & 0x01) {
					channel[7].slot[MOD].FM_KEYON (2);
				} else {
					channel[7].slot[MOD].FM_KEYOFF(2);
				}
				// SD key on/off
				if (v & 0x08) {
					channel[7].slot[CAR].FM_KEYON (2);
				} else {
					channel[7].slot[CAR].FM_KEYOFF(2);
				}
				// TOM key on/off
				if (v & 0x04) {
					channel[8].slot[MOD].FM_KEYON (2);
				} else {
					channel[8].slot[MOD].FM_KEYOFF(2);
				}
				// TOP-CY key on/off
				if (v & 0x02) {
					channel[8].slot[CAR].FM_KEYON (2);
				} else {
					channel[8].slot[CAR].FM_KEYOFF(2);
				}
			} else {
				// BD key off
				channel[6].slot[MOD].FM_KEYOFF(2);
				channel[6].slot[CAR].FM_KEYOFF(2);
				// HH key off
				channel[7].slot[MOD].FM_KEYOFF(2);
				// SD key off
				channel[7].slot[CAR].FM_KEYOFF(2);
				// TOM key off
				channel[8].slot[MOD].FM_KEYOFF(2);
				// TOP-CY off
				channel[8].slot[CAR].FM_KEYOFF(2);
			}
			return;
		}

		// keyon,block,fnum
		if ((r & 0x0F) > 8) {
			return;
		}
		unsigned chan_no = (r & 0x0F) + ch_offset;
		auto& ch  = channel[chan_no];
		int block_fnum;
		if (!(r & 0x10)) {
			// a0-a8
			block_fnum  = (ch.block_fnum & 0x1F00) | v;
		} else {
			// b0-b8
			block_fnum = ((v & 0x1F) << 8) | (ch.block_fnum & 0xFF);
			if (isExtended(chan_no)) {
				if (getFirstOfPairNum(chan_no) == chan_no) {
					// keyon/off slots of both channels
					// forming a 4-op channel
					auto& ch0 = getFirstOfPair(chan_no);
					auto& ch3 = getSecondOfPair(chan_no);
					if (v & 0x20) {
						ch0.slot[MOD].FM_KEYON(1);
						ch0.slot[CAR].FM_KEYON(1);
						ch3.slot[MOD].FM_KEYON(1);
						ch3.slot[CAR].FM_KEYON(1);
					} else {
						ch0.slot[MOD].FM_KEYOFF(1);
						ch0.slot[CAR].FM_KEYOFF(1);
						ch3.slot[MOD].FM_KEYOFF(1);
						ch3.slot[CAR].FM_KEYOFF(1);
					}
				} else {
					// do nothing
				}
			} else {
				// 2 operator function keyon/off
				if (v & 0x20) {
					ch.slot[MOD].FM_KEYON (1);
					ch.slot[CAR].FM_KEYON (1);
				} else {
					ch.slot[MOD].FM_KEYOFF(1);
					ch.slot[CAR].FM_KEYOFF(1);
				}
			}
		}
		// update
		if (ch.block_fnum != block_fnum) {
			ch.block_fnum = block_fnum;
			ch.ksl_base = ksl_tab[block_fnum >> 6];
			ch.fc       = fnumToIncrement(block_fnum);

			// BLK 2,1,0 bits -> bits 3,2,1 of kcode
			ch.kcode = (ch.block_fnum & 0x1C00) >> 9;

			// the info below is actually opposite to what is stated
			// in the Manuals (verified on real YMF262)
			// if noteSel == 0 -> lsb of kcode is bit 10 (MSB) of fnum
			// if noteSel == 1 -> lsb of kcode is bit 9 (MSB-1) of fnum
			if (nts) {
				ch.kcode |= (ch.block_fnum & 0x100) >> 8; // noteSel == 1
			} else {
				ch.kcode |= (ch.block_fnum & 0x200) >> 9; // noteSel == 0
			}
			if (isExtended(chan_no)) {
				if (getFirstOfPairNum(chan_no) == chan_no) {
					// update slots of both channels
					// forming up 4-op channel
					// refresh Total Level
					auto& ch0 = getFirstOfPair(chan_no);
					auto& ch3 = getSecondOfPair(chan_no);
					ch0.slot[MOD].TLL = ch0.slot[MOD].TL + (ch.ksl_base >> ch0.slot[MOD].ksl);
					ch0.slot[CAR].TLL = ch0.slot[CAR].TL + (ch.ksl_base >> ch0.slot[CAR].ksl);
					ch3.slot[MOD].TLL = ch3.slot[MOD].TL + (ch.ksl_base >> ch3.slot[MOD].ksl);
					ch3.slot[CAR].TLL = ch3.slot[CAR].TL + (ch.ksl_base >> ch3.slot[CAR].ksl);

					// refresh frequency counter
					ch0.slot[MOD].calc_fc(ch);
					ch0.slot[CAR].calc_fc(ch);
					ch3.slot[MOD].calc_fc(ch);
					ch3.slot[CAR].calc_fc(ch);
				} else {
					// nothing
				}
			} else {
				// refresh Total Level in both SLOTs of this channel
				ch.slot[MOD].TLL = ch.slot[MOD].TL + (ch.ksl_base >> ch.slot[MOD].ksl);
				ch.slot[CAR].TLL = ch.slot[CAR].TL + (ch.ksl_base >> ch.slot[CAR].ksl);

				// refresh frequency counter in both SLOTs of this channel
				ch.slot[MOD].calc_fc(ch);
				ch.slot[CAR].calc_fc(ch);
			}
		}
		break;
	}
	case 0xC0: {
		// CH.D, CH.C, CH.B, CH.A, FB(3bits), C
		if ((r & 0xF) > 8) {
			return;
		}
		unsigned chan_no = (r & 0x0F) + ch_offset;
		auto& ch = channel[chan_no];

		unsigned base = chan_no * 4;
		if (OPL3_mode) {
			// OPL3 mode
			pan[base + 0] = (v & 0x10) ? ~0 : 0; // ch.A
			pan[base + 1] = (v & 0x20) ? ~0 : 0; // ch.B
			pan[base + 2] = (v & 0x40) ? ~0 : 0; // ch.C
			pan[base + 3] = (v & 0x80) ? ~0 : 0; // ch.D
		} else {
			// OPL2 mode - always enabled
			pan[base + 0] = ~0; // ch.A
			pan[base + 1] = ~0; // ch.B
			pan[base + 2] = ~0; // ch.C
			pan[base + 3] = ~0; // ch.D
		}

		ch.slot[MOD].setFeedbackShift((v >> 1) & 7);
		ch.slot[MOD].CON = v & 1;

		if (isExtended(chan_no)) {
			unsigned chan_no0 = getFirstOfPairNum(chan_no);
			unsigned chan_no3 = chan_no0 + 3;
			auto& ch0 = getFirstOfPair(chan_no);
			auto& ch3 = getSecondOfPair(chan_no);
			switch ((ch0.slot[MOD].CON ? 2:0) | (ch3.slot[MOD].CON ? 1:0)) {
			case 0:
				// 1 -> 2 -> 3 -> 4 -> out
				ch0.slot[MOD].connect = &phase_modulation;
				ch0.slot[CAR].connect = &phase_modulation2;
				ch3.slot[MOD].connect = &phase_modulation;
				ch3.slot[CAR].connect = &chanOut[chan_no3];
				break;
			case 1:
				// 1 -> 2 -\.
				// 3 -> 4 --+-> out
				ch0.slot[MOD].connect = &phase_modulation;
				ch0.slot[CAR].connect = &chanOut[chan_no0];
				ch3.slot[MOD].connect = &phase_modulation;
				ch3.slot[CAR].connect = &chanOut[chan_no3];
				break;
			case 2:
				// 1 ----------\.
				// 2 -> 3 -> 4 -+-> out
				ch0.slot[MOD].connect = &chanOut[chan_no0];
				ch0.slot[CAR].connect = &phase_modulation2;
				ch3.slot[MOD].connect = &phase_modulation;
				ch3.slot[CAR].connect = &chanOut[chan_no3];
				break;
			case 3:
				// 1 -----\.
				// 2 -> 3 -+-> out
				// 4 -----/
				ch0.slot[MOD].connect = &chanOut[chan_no0];
				ch0.slot[CAR].connect = &phase_modulation2;
				ch3.slot[MOD].connect = &chanOut[chan_no3];
				ch3.slot[CAR].connect = &chanOut[chan_no3];
				break;
			}
		} else {
			// 2 operators mode
			ch.slot[MOD].connect = ch.slot[MOD].CON
			                     ? &chanOut[chan_no]
			                     : &phase_modulation;
			ch.slot[CAR].connect = &chanOut[chan_no];
		}
		break;
	}
	case 0xE0: {
		// waveform select
		int slot = slot_array[r & 0x1f];
		if (slot < 0) return;
		slot += narrow<int>(ch_offset * 2);
		auto& ch = channel[slot / 2];

		// store 3-bit value written regardless of current OPL2 or OPL3
		// mode... (verified on real YMF262)
		v &= 7;
		// ... but select only waveforms 0-3 in OPL2 mode
		if (!OPL3_mode) {
			v &= 3;
		}
		ch.slot[slot & 1].waveTable = sin.tab[v];
		break;
	}
	}
}


void YMF262::reset(EmuTime time)
{
	eg_cnt = 0;

	noise_rng = 1; // noise shift register
	nts = false; // note split
	alreadySignaledNEW2 = false;
	resetStatus(0x60);

	// reset with register write
	writeRegDirect(0x01, 0, time); // test register
	writeRegDirect(0x02, 0, time); // Timer1
	writeRegDirect(0x03, 0, time); // Timer2
	writeRegDirect(0x04, 0, time); // IRQ mask clear

	// FIX IT  registers 101, 104 and 105
	// FIX IT (dont change CH.D, CH.C, CH.B and CH.A in C0-C8 registers)
	for (int c = 0xFF; c >= 0x20; c--) {
		writeRegDirect(c, 0, time);
	}
	// FIX IT (dont change CH.D, CH.C, CH.B and CH.A in C0-C8 registers)
	for (int c = 0x1FF; c >= 0x120; c--) {
		writeRegDirect(c, 0, time);
	}

	// reset operator parameters
	for (auto& ch : channel) {
		for (auto& sl : ch.slot) {
			sl.state  = EnvelopeState::OFF;
			sl.volume = MAX_ATT_INDEX;
		}
	}

	setMixLevel(0x1b, time); // -9dB left and right
}

static unsigned calcInputRate(bool isYMF278)
{
	return unsigned(lrintf(isYMF278 ?    33868800.0f / (19 * 36)
	                                : 4 * 3579545.0f / ( 8 * 36)));
}
YMF262::YMF262(const std::string& name_,
               const DeviceConfig& config, bool isYMF278_)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "MoonSound FM-part",
	                       18, calcInputRate(isYMF278_), true)
	, debuggable(config.getMotherBoard(), getName())
	, timer1(isYMF278_
	         ? EmuTimer::createOPL4_1(config.getScheduler(), *this)
	         : EmuTimer::createOPL3_1(config.getScheduler(), *this))
	, timer2(isYMF278_
	         ? EmuTimer::createOPL4_2(config.getScheduler(), *this)
	         : EmuTimer::createOPL3_2(config.getScheduler(), *this))
	, irq(config.getMotherBoard(), getName() + ".IRQ")
	, isYMF278(isYMF278_)
{
	// For debugging: print out tables to be able to compare before/after
	// when the calculation changes.
	if (false) {
		for (const auto& e : tlTab) std::cout << e << '\n';
		std::cout << '\n';
		for (const auto& t : sin.tab) {
			for (const auto& e : t) {
				std::cout << e << '\n';
			}
		}
	}

	registerSound(config);
	reset(config.getMotherBoard().getCurrentTime()); // must come after registerSound() because of call to setSoftwareVolume() via setMixLevel()
}

YMF262::~YMF262()
{
	unregisterSound();
}

uint8_t YMF262::readStatus()
{
	// no need to call updateStream(time)
	uint8_t result = status | status2;
	status2 = 0;
	return result;
}

uint8_t YMF262::peekStatus() const
{
	return status | status2;
}

bool YMF262::checkMuteHelper() const
{
	// TODO this doesn't always mute when possible
	for (const auto& ch : channel) {
		for (const auto& sl : ch.slot) {
			if (!((sl.state == EnvelopeState::OFF) ||
			      ((sl.state == EnvelopeState::RELEASE) &&
			       ((narrow<int>(sl.TLL) + sl.volume) >= ENV_QUIET)))) {
				return false;
			}
		}
	}
	return true;
}

void YMF262::setMixLevel(uint8_t x, EmuTime time)
{
	// Only present on YMF278
	// see mix_level[] and vol_factor() in YMF278.cc
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

float YMF262::getAmplificationFactorImpl() const
{
	return 1.0f / 4096.0f;
}

void YMF262::generateChannels(std::span<float*> bufs, unsigned num)
{
	// TODO implement per-channel mute (instead of all-or-nothing)
	// TODO output rhythm on separate channels?
	if (checkMuteHelper()) {
		// TODO update internal state, even if muted
		std::ranges::fill(bufs, nullptr);
		return;
	}

	bool rhythmEnabled = (rhythm & 0x20) != 0;

	for (auto j : xrange(num)) {
		// Amplitude modulation: 27 output levels (triangle waveform);
		// 1 level takes one of: 192, 256 or 448 samples
		// One entry from LFO_AM_TABLE lasts for 64 samples
		lfo_am_cnt.addQuantum();
		if (lfo_am_cnt == LFOAMIndex(LFO_AM_TAB_ELEMENTS)) {
			// lfo_am_table is 210 elements long
			lfo_am_cnt = LFOAMIndex(0);
		}
		unsigned tmp = lfo_am_table[lfo_am_cnt.toInt()];
		unsigned lfo_am = lfo_am_depth ? tmp : tmp / 4;

		// clear channel outputs
		std::ranges::fill(chanOut, 0);

		// channels 0,3 1,4 2,5  9,12 10,13 11,14
		// in either 2op or 4op mode
		for (int k = 0; k <= 9; k += 9) {
			for (auto i : xrange(3)) {
				auto& ch0 = channel[k + i + 0];
				auto& ch3 = channel[k + i + 3];
				// extended 4op ch#0 part 1 or 2op ch#0
				ch0.chan_calc(lfo_am);
				if (ch0.extended) {
					// extended 4op ch#0 part 2
					ch3.chan_calc_ext(lfo_am);
				} else {
					// standard 2op ch#3
					ch3.chan_calc(lfo_am);
				}
			}
		}

		// channels 6,7,8 rhythm or 2op mode
		if (!rhythmEnabled) {
			channel[6].chan_calc(lfo_am);
			channel[7].chan_calc(lfo_am);
			channel[8].chan_calc(lfo_am);
		} else {
			// Rhythm part
			chan_calc_rhythm(lfo_am);
		}

		// channels 15,16,17 are fixed 2-operator channels only
		channel[15].chan_calc(lfo_am);
		channel[16].chan_calc(lfo_am);
		channel[17].chan_calc(lfo_am);

		for (auto i : xrange(18)) {
			bufs[i][2 * j + 0] += narrow_cast<float>(chanOut[i] & pan[4 * i + 0]);
			bufs[i][2 * j + 1] += narrow_cast<float>(chanOut[i] & pan[4 * i + 1]);
			// unused c        += narrow_cast<float>(chanOut[i] & pan[4 * i + 2]);
			// unused d        += narrow_cast<float>(chanOut[i] & pan[4 * i + 3]);
		}

		advance();
	}
}


static constexpr std::initializer_list<enum_string<YMF262::EnvelopeState>> envelopeStateInfo = {
	{ "ATTACK",  YMF262::EnvelopeState::ATTACK  },
	{ "DECAY",   YMF262::EnvelopeState::DECAY   },
	{ "SUSTAIN", YMF262::EnvelopeState::SUSTAIN },
	{ "RELEASE", YMF262::EnvelopeState::RELEASE },
	{ "OFF",     YMF262::EnvelopeState::OFF     }
};
SERIALIZE_ENUM(YMF262::EnvelopeState, envelopeStateInfo);

template<typename Archive>
void YMF262::Slot::serialize(Archive& a, unsigned /*version*/)
{
	// waveTable
	auto waveform = unsigned((waveTable.data() - sin.tab[0].data()) / SIN_LEN);
	a.serialize("waveform", waveform);
	if constexpr (Archive::IS_LOADER) {
		waveTable = sin.tab[waveform];
	}

	// done by rewriting registers:
	//   connect, fb_shift, CON
	// TODO handle more state like this

	a.serialize("Cnt",       Cnt,
	            "Incr",      Incr,
	            "op1_out",   op1_out,
	            "TL",        TL,
	            "TLL",       TLL,
	            "volume",    volume,
	            "sl",        sl,
	            "state",     state,
	            "eg_m_ar",   eg_m_ar,
	            "eg_m_dr",   eg_m_dr,
	            "eg_m_rr",   eg_m_rr,
	            "eg_sh_ar",  eg_sh_ar,
	            "eg_sel_ar", eg_sel_ar,
	            "eg_sh_dr",  eg_sh_dr,
	            "eg_sel_dr", eg_sel_dr,
	            "eg_sh_rr",  eg_sh_rr,
	            "eg_sel_rr", eg_sel_rr,
	            "key",       key,
	            "eg_type",   eg_type,
	            "AMmask",    AMmask,
	            "vib",       vib,
	            "ar",        ar,
	            "dr",        dr,
	            "rr",        rr,
	            "KSR",       KSR,
	            "ksl",       ksl,
	            "ksr",       ksr,
	            "mul",       mul);
}

template<typename Archive>
void YMF262::Channel::serialize(Archive& a, unsigned /*version*/)
{
	a.serialize("slots",      slot,
	            "block_fnum", block_fnum,
	            "fc",         fc,
	            "ksl_base",   ksl_base,
	            "kcode",      kcode,
	            "extended",   extended);
}

// version 1: initial version
// version 2: added alreadySignaledNEW2
template<typename Archive>
void YMF262::serialize(Archive& a, unsigned version)
{
	a.serialize("timer1",  *timer1,
	            "timer2",  *timer2,
	            "irq",     irq,
	            "chanout", chanOut);
	a.serialize_blob("registers", reg);
	a.serialize("channels",           channel,
	            "eg_cnt",             eg_cnt,
	            "noise_rng",          noise_rng,
	            "lfo_am_cnt",         lfo_am_cnt,
	            "lfo_pm_cnt",         lfo_pm_cnt,
	            "lfo_am_depth",       lfo_am_depth,
	            "lfo_pm_depth_range", lfo_pm_depth_range,
	            "rhythm",             rhythm,
	            "nts",                nts,
	            "OPL3_mode",          OPL3_mode,
	            "status",             status,
	            "status2",            status2,
	            "statusMask",         statusMask);
	if (a.versionAtLeast(version, 2)) {
		a.serialize("alreadySignaledNEW2", alreadySignaledNEW2);
	} else {
		assert(Archive::IS_LOADER);
		alreadySignaledNEW2 = true; // we can't know the actual value,
									// but 'true' is the safest value
	}

	// TODO restore more state by rewriting register values
	//   this handles pan
	EmuTime time = timer1->getCurrentTime();
	for (auto i : xrange(0xC0, 0xC9)) {
		writeRegDirect(i + 0x000, reg[i + 0x000], time);
		writeRegDirect(i + 0x100, reg[i + 0x100], time);
	}
}

INSTANTIATE_SERIALIZE_METHODS(YMF262);


// YMF262::Debuggable

YMF262::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                               const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs",
	                   "MoonSound FM-part registers", 0x200)
{
}

uint8_t YMF262::Debuggable::read(unsigned address)
{
	const auto& ymf262 = OUTER(YMF262, debuggable);
	return ymf262.peekReg(address);
}

void YMF262::Debuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& ymf262 = OUTER(YMF262, debuggable);
	ymf262.writeReg512(address, value, time);
}

} // namespace openmsx
