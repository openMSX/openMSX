/*
* Original copyright:
* -------------------------------------------------------------------
*    Copyright (C) 2019 Nuke.YKT
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*
*     Yamaha YM2413 emulator
*     Thanks:
*         siliconpr0n.org(digshadow, John McMaster):
*             VRC VII decap and die shot.
*
*     version: 0.9
* -------------------------------------------------------------------
*
* See YM2413NukeYKT.hh for more info.
*/

#include "YM2413NukeYKT.hh"
#include "serialize.hh"
#include "cstd.hh"
#include "enumerate.hh"
#include "Math.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cstring>

namespace openmsx {
namespace YM2413NukeYKT {

[[nodiscard]] static constexpr bool is_rm_cycle(int cycle)
{
	return (11 <= cycle) && (cycle <= 16);
}
[[nodiscard]] static constexpr YM2413::RmNum rm_for_cycle(int cycle)
{
	return static_cast<YM2413::RmNum>(cycle - 11);
}

static constexpr auto logSinTab = [] {
	std::array<uint16_t, 256> result = {};
	//for (auto [i, r] : enumerate(result)) { msvc bug
	for (int i = 0; i < 256; ++i) {
		result[i] = narrow_cast<uint16_t>(cstd::round(-cstd::log2<8, 3>(cstd::sin<2>((double(i) + 0.5) * Math::pi / 256.0 / 2.0)) * 256.0));
	}
	return result;
}();
static constexpr auto expTab = [] {
	std::array<uint16_t, 256> result = {};
	//for (auto [i, r] : enumerate(result)) { msvc bug
	for (int i = 0; i < 256; ++i) {
		result[i] = narrow_cast<uint16_t>(cstd::round((cstd::exp2<6>(double(255 - i) / 256.0)) * 1024.0));
	}
	return result;
}();

constexpr std::array<YM2413::Patch, 15> YM2413::m_patches = {
	YM2413::Patch{0x1e, 2, 7, {0, 0}, {1, 1}, {1, 1}, {1, 0}, {0x1, 0x1}, {0, 0}, {0xd, 0x7}, {0x0, 0x8}, {0x0, 0x1}, {0x0, 0x7}},
	YM2413::Patch{0x1a, 1, 5, {0, 0}, {0, 1}, {0, 0}, {1, 0}, {0x3, 0x1}, {0, 0}, {0xd, 0xf}, {0x8, 0x7}, {0x2, 0x1}, {0x3, 0x3}},
	YM2413::Patch{0x19, 0, 0, {0, 0}, {0, 0}, {0, 0}, {1, 0}, {0x3, 0x1}, {2, 0}, {0xf, 0xc}, {0x2, 0x4}, {0x1, 0x2}, {0x1, 0x3}},
	YM2413::Patch{0x0e, 0, 7, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0x1, 0x1}, {0, 0}, {0xa, 0x6}, {0x8, 0x4}, {0x7, 0x2}, {0x0, 0x7}},
	YM2413::Patch{0x1e, 0, 6, {0, 0}, {0, 0}, {1, 1}, {1, 0}, {0x2, 0x1}, {0, 0}, {0xe, 0x7}, {0x0, 0x6}, {0x0, 0x2}, {0x0, 0x8}},
	YM2413::Patch{0x16, 0, 5, {0, 0}, {0, 0}, {1, 1}, {1, 0}, {0x1, 0x2}, {0, 0}, {0xe, 0x7}, {0x0, 0x1}, {0x0, 0x1}, {0x0, 0x8}},
	YM2413::Patch{0x1d, 0, 7, {0, 0}, {0, 1}, {1, 1}, {0, 0}, {0x1, 0x1}, {0, 0}, {0x8, 0x8}, {0x2, 0x1}, {0x1, 0x0}, {0x0, 0x7}},
	YM2413::Patch{0x2d, 2, 4, {0, 0}, {0, 0}, {1, 1}, {0, 0}, {0x3, 0x1}, {0, 0}, {0xa, 0x7}, {0x2, 0x2}, {0x0, 0x0}, {0x0, 0x7}},
	YM2413::Patch{0x1b, 0, 6, {0, 0}, {1, 1}, {1, 1}, {0, 0}, {0x1, 0x1}, {0, 0}, {0x6, 0x6}, {0x4, 0x5}, {0x1, 0x1}, {0x0, 0x7}},
	YM2413::Patch{0x0b, 3, 0, {0, 0}, {1, 1}, {0, 1}, {0, 0}, {0x1, 0x1}, {0, 0}, {0x8, 0xf}, {0x5, 0x7}, {0x7, 0x0}, {0x1, 0x7}},
	YM2413::Patch{0x03, 2, 1, {0, 0}, {0, 0}, {0, 0}, {1, 0}, {0x3, 0x1}, {2, 0}, {0xf, 0xe}, {0xa, 0x4}, {0x1, 0x0}, {0x0, 0x4}},
	YM2413::Patch{0x24, 0, 7, {0, 1}, {0, 1}, {0, 0}, {1, 0}, {0x7, 0x1}, {0, 0}, {0xf, 0xf}, {0x8, 0x8}, {0x2, 0x1}, {0x2, 0x2}},
	YM2413::Patch{0x0c, 0, 5, {0, 0}, {1, 1}, {1, 0}, {0, 1}, {0x1, 0x0}, {0, 0}, {0xc, 0xf}, {0x2, 0x5}, {0x2, 0x4}, {0x0, 0x2}},
	YM2413::Patch{0x15, 0, 3, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x1, 0x1}, {1, 0}, {0xc, 0x9}, {0x9, 0x5}, {0x0, 0x0}, {0x3, 0x2}},
	YM2413::Patch{0x09, 0, 3, {0, 0}, {1, 1}, {1, 0}, {0, 0}, {0x1, 0x1}, {2, 0}, {0xf, 0xe}, {0x1, 0x4}, {0x4, 0x1}, {0x0, 0x3}},
};

constexpr array_with_enum_index<YM2413::RmNum, YM2413::Patch> YM2413::r_patches = {
	YM2413::Patch{0x18, 1, 7, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x1, 0x0}, {0, 0}, {0xd, 0x0}, {0xf, 0x0}, {0x6, 0x0}, {0xa, 0x0}},
	YM2413::Patch{0x00, 0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x1, 0x0}, {0, 0}, {0xc, 0x0}, {0x8, 0x0}, {0xa, 0x0}, {0x7, 0x0}},
	YM2413::Patch{0x00, 0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x5, 0x0}, {0, 0}, {0xf, 0x0}, {0x8, 0x0}, {0x5, 0x0}, {0x9, 0x0}},
	YM2413::Patch{0x00, 0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x0, 0x1}, {0, 0}, {0x0, 0xf}, {0x0, 0x8}, {0x0, 0x6}, {0x0, 0xd}},
	YM2413::Patch{0x00, 0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x0, 0x1}, {0, 0}, {0x0, 0xd}, {0x0, 0x8}, {0x0, 0x4}, {0x0, 0x8}},
	YM2413::Patch{0x00, 0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x0, 0x1}, {0, 0}, {0x0, 0xa}, {0x0, 0xa}, {0x0, 0x5}, {0x0, 0x5}},
};

static constexpr std::array<uint8_t, 18> CH_OFFSET = {
	1, 2, 0, 1, 2, 3, 4, 5, 3, 4, 5, 6, 7, 8, 6, 7, 8, 0
};

static constexpr std::array<int8_t, 8> VIB_TAB = {0, 1, 2, 1, 0, -1, -2, -1};

// Define the tables
//    constexpr uint8_t attack[14][4][64] = { ... };
//    constexpr uint8_t releaseIndex[14][4][4] = { ... };
//    constexpr uint8_t releaseData[64][64] = { ... };
// Theoretically these could all be initialized via some constexpr functions.
// The calculation isn't difficult, but it's a bit long. 'clang' can handle it,
// but 'gcc' cannot. So instead, for now, we pre-calculate these tables and
// #include them. See 'generateNukeYktTables.cpp' for the generator code.
#include "YM2413NukeYktTables.ii"


YM2413::YM2413()
	: attackPtr (/*dummy*/attack[0][0])
	, releasePtr(/*dummy*/releaseData[0])
{
	// copy ROM patches to array (for faster lookup)
	ranges::copy(m_patches, subspan(patches, 1));
	reset();
}

void YM2413::reset()
{
	for (auto& w : writes) w.port = uint8_t(-1);
	write_fm_cycle = uint8_t(-1);
	write_data = fm_data = write_address = 0;
	fast_fm_rewrite = test_mode_active = false;

	eg_counter_state = 3;
	eg_timer = eg_timer_shift = eg_timer_shift_lock = eg_timer_lock = 0;
	attackPtr  = attack[eg_timer_shift_lock][eg_timer_lock];
	auto idx = releaseIndex[eg_timer_shift_lock][eg_timer_lock][eg_counter_state];
	releasePtr = releaseData[idx];
	ranges::fill(eg_state, EgState::release);
	ranges::fill(eg_level, 0x7f);
	ranges::fill(eg_dokon, false);
	eg_rate[0] = eg_rate[1] = 0;
	eg_sl[0] = eg_sl[1] = eg_out[0] = eg_out[1] = 0;
	eg_timer_shift_stop = false;
	eg_kon[0] = eg_kon[1] = eg_off[0] = eg_off[1] = false;

	ranges::fill(pg_phase, 0);
	ranges::fill(op_fb1, 0);
	ranges::fill(op_fb2, 0);

	op_mod = 0;
	op_phase[0] = op_phase[1] = 0;

	lfo_counter = lfo_am_counter = 0;
	lfo_vib_counter = lfo_am_out = 0;
	lfo_vib = VIB_TAB[lfo_vib_counter];
	lfo_am_step = lfo_am_dir = false;

	ranges::fill(fnum, 0);
	ranges::fill(block, 0);
	ranges::fill(vol8, 0);
	ranges::fill(inst, 0);
	ranges::fill(sk_on, 0);
	for (auto i : xrange(9)) {
		p_inst[i] = &patches[inst[i]];
		changeFnumBlock(i);
	}
	rhythm = testMode = 0;
	patches[0] = Patch(); // reset user patch, leave ROM patches alone
	c_dcm[0] = c_dcm[1] = c_dcm[2] = 0;

	rm_noise = 1; // any value except 0 (1 keeps the output the same as the original code)
	rm_tc_bits = 0;

	delay6 = delay7 = delay10 = delay11 = delay12 = 0;

	ranges::fill(regs, 0);
	latch = 0;
}

template<uint32_t CYCLES> ALWAYS_INLINE const YM2413::Patch& YM2413::preparePatch1(bool use_rm_patches) const
{
	return (is_rm_cycle(CYCLES) && use_rm_patches)
	     ? r_patches[rm_for_cycle(CYCLES)]
	     : *p_inst[CH_OFFSET[CYCLES]];
}

template<uint32_t CYCLES> ALWAYS_INLINE uint32_t YM2413::envelopeKSLTL(const Patch& patch1, bool use_rm_patches) const
{
	constexpr uint32_t mcsel = ((CYCLES + 1) / 3) & 1;
	constexpr uint32_t ch = CH_OFFSET[CYCLES];

	auto ksl = uint32_t(p_ksl[ch]) >> patch1.ksl_t[mcsel];
	auto tl2 = [&]() -> uint32_t {
		if ((rm_for_cycle(CYCLES) == one_of(RmNum::hh, RmNum::tom)) && use_rm_patches) {
			return inst[ch] << (2 + 1);
		} else if /*constexpr*/ (mcsel == 1) { // constexpr triggers compile error on visual studio
			return vol8[ch];
		} else {
			return patch1.tl2;
		}
	}();
	return ksl + tl2;
}

template<uint32_t CYCLES> ALWAYS_INLINE void YM2413::envelopeTimer1()
{
	if constexpr (CYCLES == 0) {
		eg_counter_state = (eg_counter_state + 1) & 3;
		auto idx = releaseIndex[eg_timer_shift_lock][eg_timer_lock][eg_counter_state];
		releasePtr = releaseData[idx];
	}
}

template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE void YM2413::envelopeTimer2(bool& eg_timer_carry)
{
	if constexpr (TEST_MODE) {
		if (CYCLES == 0 && (eg_counter_state & 1) == 0) {
			eg_timer_lock = eg_timer & 3;
			eg_timer_shift_lock = /*likely*/(eg_timer_shift <= 13) ? eg_timer_shift : 0;
			eg_timer_shift = 0;

			attackPtr  = attack[eg_timer_shift_lock][eg_timer_lock];
			auto idx = releaseIndex[eg_timer_shift_lock][eg_timer_lock][eg_counter_state];
			releasePtr = releaseData[idx];
		}
		{ // EG timer
			bool timer_inc = (eg_counter_state != 3) ? false
				       : (CYCLES == 0)           ? true
								 : eg_timer_carry;
			auto timer_bit = (eg_timer & 1) + timer_inc;
			eg_timer_carry = timer_bit & 2;
			eg_timer = ((timer_bit & 1) << 17) | (eg_timer >> 1);
			if (testMode & 8) {
				const auto& write = writes[CYCLES];
				auto data = (write.port != uint8_t(-1)) ? write.value : write_data;
				eg_timer &= 0x2ffff;
				eg_timer |= (data << (16 - 2)) & 0x10000;
			}
		}
		if constexpr (CYCLES == 0) {
			eg_timer_shift_stop = false;
		} else if (!eg_timer_shift_stop && ((eg_timer >> 16) & 1)) {
			eg_timer_shift = CYCLES;
			eg_timer_shift_stop = true;
		}
	} else {
		if constexpr (CYCLES == 0) {
			if ((eg_counter_state & 1) == 0) {
				eg_timer_lock = eg_timer & 3;
				eg_timer_shift_lock = (eg_timer_shift > 13) ? 0 : eg_timer_shift;

				attackPtr  = attack[eg_timer_shift_lock][eg_timer_lock];
				auto idx = releaseIndex[eg_timer_shift_lock][eg_timer_lock][eg_counter_state];
				releasePtr = releaseData[idx];
			}
			if (eg_counter_state == 3) {
				eg_timer = (eg_timer + 1) & 0x3ffff;
				eg_timer_shift = narrow_cast<uint8_t>(Math::findFirstSet(eg_timer));
			}
		}
	}
}

template<uint32_t CYCLES> ALWAYS_INLINE bool YM2413::envelopeGenerate1()
{
	int32_t level = eg_level[(CYCLES + 16) % 18];
	bool prev2_eg_off   = eg_off[(CYCLES + 16) & 1];
	bool prev2_eg_kon   = eg_kon[(CYCLES + 16) & 1];
	bool prev2_eg_dokon = eg_dokon[(CYCLES + 16) % 18];

	using enum EgState;
	auto state = eg_state[(CYCLES + 16) % 18];
	if (prev2_eg_dokon) [[unlikely]] {
		eg_state[(CYCLES + 16) % 18] = attack;
	} else if (!prev2_eg_kon) {
		eg_state[(CYCLES + 16) % 18] = release;
	} else if ((state == attack) && (level == 0)) [[unlikely]] {
		eg_state[(CYCLES + 16) % 18] = decay;
	} else if ((state == decay) && ((level >> 3) == eg_sl[CYCLES & 1])) [[unlikely]] {
		eg_state[(CYCLES + 16) % 18] = sustain;
	}

	auto prev2_rate = eg_rate[(CYCLES + 16) & 1];
	int32_t next_level
	    = (state != attack && prev2_eg_off && !prev2_eg_dokon) ? 0x7f
	    : ((prev2_rate >= 60) && prev2_eg_dokon)                        ? 0x00
	                                                                    : level;
	auto step = [&]() -> int {
		switch (state) {
		case attack:
			if (prev2_eg_kon && (level != 0)) [[likely]] {
				return (level ^ 0xfff) >> attackPtr[prev2_rate];
			}
			break;
		case decay:
			if ((level >> 3) == eg_sl[CYCLES & 1]) return 0;
			[[fallthrough]];
		case sustain:
		case release:
			if (!prev2_eg_off && !prev2_eg_dokon) {
				return releasePtr[prev2_rate];
			}
			break;
		default:
			UNREACHABLE;
		}
		return 0;
	}();
	eg_level[(CYCLES + 16) % 18] = narrow_cast<uint8_t>(next_level + step);

	return level == 0x7f; // eg_silent
}

template<uint32_t CYCLES> ALWAYS_INLINE void YM2413::envelopeGenerate2(const Patch& patch1, bool use_rm_patches)
{
	constexpr uint32_t mcsel = ((CYCLES + 1) / 3) & 1;
	constexpr uint32_t ch = CH_OFFSET[CYCLES];

	bool new_eg_off = eg_level[CYCLES] >= 124; // (eg_level[CYCLES] >> 2) == 0x1f;
	eg_off[CYCLES & 1] = new_eg_off;

	auto sk = sk_on[CH_OFFSET[CYCLES]];
	bool new_eg_kon = [&]() {
		bool result = sk & 1;
		if (is_rm_cycle(CYCLES) && use_rm_patches) {
			switch (rm_for_cycle(CYCLES)) {
				using enum RmNum;
				case bd0:
				case bd1:
					result |= bool(rhythm & 0x10);
					break;
				case sd:
					result |= bool(rhythm & 0x08);
					break;
				case tom:
					result |= bool(rhythm & 0x04);
					break;
				case tc:
					result |= bool(rhythm & 0x02);
					break;
				case hh:
					result |= bool(rhythm & 0x01);
					break;
				default:
					break; // suppress warning
			}
		}
		return result;
	}();
	eg_kon[CYCLES & 1] = new_eg_kon;

	// Calculate rate
	using enum EgState;
	auto state_rate = eg_state[CYCLES];
	if (state_rate == release && new_eg_kon && new_eg_off) [[unlikely]] {
		state_rate = attack;
		eg_dokon[CYCLES] = true;
	} else {
		eg_dokon[CYCLES] = false;
	}

	auto rate4 = [&]() {
		if (!new_eg_kon && !(sk & 2) && mcsel == 1 && !patch1.et[mcsel]) {
			return 7 * 4;
		}
		if (new_eg_kon && eg_state[CYCLES] == release && !new_eg_off) {
			return 12 * 4;
		}
		bool tom_or_hh = (rm_for_cycle(CYCLES) == one_of(RmNum::tom, RmNum::hh)) && use_rm_patches;
		if (!new_eg_kon && !mcsel && !tom_or_hh) {
			return 0 * 4;
		}
		return (state_rate == attack ) ?   patch1.ar4[mcsel]
		   :   (state_rate == decay  ) ?   patch1.dr4[mcsel]
		   :   (state_rate == sustain) ?   (patch1.et[mcsel] ? (0 * 4) : patch1.rr4[mcsel])
		   : /*(state_rate == release) ?*/ ((sk & 2) ? (5 * 4) : patch1.rr4[mcsel]);
	}();

	eg_rate[CYCLES & 1] = narrow_cast<uint8_t>([&]() {
		if (rate4 == 0) return 0;
		auto tmp = rate4 + (p_ksr_freq[ch] >> patch1.ksr_t[mcsel]);
		return (tmp < 0x40) ? tmp
		                    : (0x3c | (tmp & 3));
	}());
}

template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE void YM2413::doLFO(bool& lfo_am_car)
{
	if constexpr (TEST_MODE) {
		// Update counter
		if constexpr (CYCLES == 17) {
			lfo_counter++;
			if (((lfo_counter & 0x3ff) == 0) || (testMode & 8)) {
				lfo_vib_counter = (lfo_vib_counter + 1) & 7;
				lfo_vib = VIB_TAB[lfo_vib_counter];
			}
			lfo_am_step = (lfo_counter & 0x3f) == 0;
		}

		// LFO AM
		auto am_inc = ((lfo_am_step || (testMode & 8)) && CYCLES < 9)
		            ? (lfo_am_dir | (CYCLES == 0))
		            : 0;

		if constexpr (CYCLES == 0) {
			if (lfo_am_dir && (lfo_am_counter & 0x7f) == 0) {
				lfo_am_dir = false;
			} else if (!lfo_am_dir && (lfo_am_counter & 0x69) == 0x69) {
				lfo_am_dir = true;
			}
		}

		auto am_bit = (lfo_am_counter & 1) + am_inc + ((CYCLES < 9) ? lfo_am_car : false);
		if constexpr (CYCLES < 8) {
			lfo_am_car = am_bit & 2;
		}
		lfo_am_counter = uint16_t(((am_bit & 1) << 8) | (lfo_am_counter >> 1));

		// Reset LFO
		if (testMode & 2) {
			lfo_vib_counter = 0;
			lfo_vib = VIB_TAB[lfo_vib_counter];
			lfo_counter = 0;
			lfo_am_dir = false;
			lfo_am_counter &= 0xff;
		}
	} else {
		if constexpr (CYCLES == 17) {
			int delta = lfo_am_dir ? -1 : 1;

			if (lfo_am_dir && (lfo_am_counter & 0x7f) == 0) {
				lfo_am_dir = false;
			} else if (!lfo_am_dir && (lfo_am_counter & 0x69) == 0x69) {
				lfo_am_dir = true;
			}

			if (lfo_am_step) {
				lfo_am_counter = (lfo_am_counter + delta) & 0x1ff;
			}

			lfo_counter++;
			if ((lfo_counter & 0x3ff) == 0) {
				lfo_vib_counter = (lfo_vib_counter + 1) & 7;
				lfo_vib = VIB_TAB[lfo_vib_counter];
			}
			lfo_am_step = (lfo_counter & 0x3f) == 0;
		}
	}
	if constexpr (CYCLES == 17) {
		lfo_am_out = (lfo_am_counter >> 3) & 0x0f;
	}
}

template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE void YM2413::doRhythm()
{
	if constexpr (TEST_MODE) {
		bool nbit = (rm_noise ^ (rm_noise >> 14)) & 1;
		nbit |= bool(testMode & 2);
		rm_noise = (nbit << 22) | (rm_noise >> 1);
	} else {
		// When test-mode does not interfere, the formula for a single step is:
		//    x = ((x & 1) << 22) ^ ((x & 0x4000) << 8) ^ (x >> 1);
		// From this we can easily derive the value of the noise bit (bit 0)
		// 13 and 16 steps in the future (see getPhase()). We can also
		// derive a formula that takes 18 steps at once.
		if constexpr (CYCLES == 17) {
			rm_noise = ((rm_noise & 0x1ff) << 14)
			         ^ ((rm_noise & 0x3ffff) << 5)
			         ^ (rm_noise & 0x7fc000)
			         ^ ((rm_noise >> 9) & 0x3fe0)
			         ^ (rm_noise >> 18);
		}
	}
}

template<uint32_t CYCLES> ALWAYS_INLINE uint32_t YM2413::getPhaseMod(uint8_t fb_t)
{
	bool ismod2 = ((rhythm & 0x20) && (CYCLES == one_of(12u, 13u)))
	            ? false
	            : (((CYCLES + 4) / 3) & 1);
	bool ismod3 = ((rhythm & 0x20) && (CYCLES == one_of(15u, 16u)))
	            ? false
	            : (((CYCLES + 1) / 3) & 1);

	if (ismod3) return op_mod << 1;
	if (ismod2) {
		constexpr uint32_t cycles9 = (CYCLES + 3) % 9;
		uint32_t op_fbsum = (op_fb1[cycles9] + op_fb2[cycles9]) & 0x7fffffff;
		return op_fbsum >> fb_t;
	}
	return 0;
}

template<uint32_t CYCLES> ALWAYS_INLINE void YM2413::doRegWrite()
{
	if (write_fm_cycle == CYCLES) [[unlikely]] {
		doRegWrite(CYCLES % 9);
	}
}

void YM2413::changeFnumBlock(uint32_t ch)
{
	static constexpr std::array<uint8_t, 16> KSL_TABLE = {
		0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
	};
	p_ksl[ch] = narrow_cast<uint8_t>(std::max(0, KSL_TABLE[fnum[ch] >> 5] - ((8 - block[ch]) << 3)) << 1);
	p_incr[ch] = narrow_cast<uint16_t>(fnum[ch] << block[ch]);
	p_ksr_freq[ch] = uint8_t((block[ch] << 1) | (fnum[ch] >> 8));
}

NEVER_INLINE void YM2413::doRegWrite(uint8_t channel)
{
	if (write_address < 0x40) {
		write_fm_cycle = uint8_t(-1);
		doRegWrite(write_address & 0xf0, channel, fm_data);
	} else {
		write_address -= 0x40; // try again in 18 steps
	}
}

void YM2413::doRegWrite(uint8_t regBlock, uint8_t channel, uint8_t data)
{
	switch (regBlock) {
	case 0x10:
		fnum[channel] = uint16_t((fnum[channel] & 0x100) | data);
		changeFnumBlock(channel);
		break;
	case 0x20:
		fnum[channel] = uint16_t((fnum[channel] & 0xff) | ((data & 1) << 8));
		block[channel] = (data >> 1) & 7;
		changeFnumBlock(channel);
		sk_on[channel] = (data >> 4) & 3;
		break;
	case 0x30:
		vol8[channel] = uint8_t((data & 0x0f) << (2 + 1)); // pre-multiply by 8
		inst[channel] = (data >> 4) & 0x0f;
		p_inst[channel] = &patches[inst[channel]];
		break;
	}
}

template<uint32_t CYCLES> ALWAYS_INLINE void YM2413::doIO()
{
	if (writes[CYCLES].port != uint8_t(-1)) [[unlikely]] {
		doIO((CYCLES + 1) % 18, writes[CYCLES]);
	}
}

NEVER_INLINE void YM2413::doIO(uint32_t cycles_plus_1, Write& write)
{
	write_data = write.value;
	if (write.port) {
		// data
		if (write_address < 0x10) {
			doModeWrite(write_address, write.value);
		} else if (write_address < 0x40) {
			write_fm_cycle = write_address & 0xf;
			fm_data = write.value;
			if (!fast_fm_rewrite && (write_fm_cycle == cycles_plus_1)) {
				write_address += 0x40; // postpone for 18 steps
			}
			// First fm-register write takes one cycle longer than
			// subsequent writes to the same register. When the address
			// is changed (even if to the same value) then there's again
			// one cycle penalty.
			fast_fm_rewrite = true;
		}
	} else {
		// address
		write_address = write.value;
		write_fm_cycle = uint8_t(-1); // cancel pending fm write
		fast_fm_rewrite = false;
	}

	write.port = uint8_t(-1);
}

void YM2413::doModeWrite(uint8_t address, uint8_t value)
{
	auto slot = address & 1;
	switch (address) {
	case 0x00:
	case 0x01:
		patches[0].setMulti(slot, value & 0x0f);
		patches[0].setKSR(slot, (value >> 4) & 1);
		patches[0].et[slot]  = (value >> 5) & 1;
		patches[0].vib[slot] = (value >> 6) & 1;
		patches[0].setAM(slot, (value >> 7) & 1);
		break;

	case 0x02:
		patches[0].setKSL(0, (value >> 6) & 3);
		patches[0].setTL(value & 0x3f);
		break;

	case 0x03:
		patches[0].setKSL(1, (value >> 6) & 3);
		patches[0].dcm = (value >> 3) & 3;
		patches[0].setFB(value & 7);
		break;

	case 0x04:
	case 0x05:
		patches[0].setDR(slot, value & 0x0f);
		patches[0].setAR(slot, (value >> 4) & 0x0f);
		break;

	case 0x06:
	case 0x07:
		patches[0].setRR(slot, value & 0x0f);
		patches[0].sl[slot] = (value >> 4) & 0x0f;
		break;

	case 0x0e:
		rhythm = value & 0x3f;
		break;

	case 0x0f:
		testMode = value & 0x0f;
		break;
	}
}

template<uint32_t CYCLES> ALWAYS_INLINE void YM2413::doOperator(std::span<float*, 9 + 5> out, bool eg_silent)
{
	bool ismod1 = ((rhythm & 0x20) && (CYCLES == one_of(14u, 15u)))
	            ? false
	            : (((CYCLES + 2) / 3) & 1);
	constexpr bool is_next_mod3 = ((CYCLES + 2) / 3) & 1; // approximate: will 'ismod3' possibly be true next step

	auto output = [&]() -> int32_t {
		if (eg_silent) return 0;
		auto prev2_phase = op_phase[(CYCLES - 2) & 1];
		auto quarter = narrow_cast<uint8_t>((prev2_phase & 0x100) ? ~prev2_phase : prev2_phase);
		auto logSin = logSinTab[quarter];
		auto op_level = std::min(4095, logSin + (eg_out[(CYCLES - 2) & 1] << 4));
		uint32_t op_exp_m = expTab[op_level & 0xff];
		auto op_exp_s = op_level >> 8;
		if (prev2_phase & 0x200) {
			return /*unlikely*/(c_dcm[(CYCLES + 16) % 3] & (ismod1 ? 1 : 2))
			       ? ~0
			       : ~(op_exp_m >> op_exp_s);
		} else {
			return narrow<int32_t>(op_exp_m >> op_exp_s);
		}
	}();

	if (ismod1) {
		constexpr uint32_t cycles9 = (CYCLES + 1) % 9;
		op_fb2[cycles9] = op_fb1[cycles9];
		op_fb1[cycles9] = narrow_cast<int16_t>(output);
	}
	channelOutput<CYCLES>(out, output >> 3);

	if (is_next_mod3) {
		op_mod = output & 0x1ff;
	}
}

template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE uint32_t YM2413::getPhase(uint8_t& rm_hh_bits)
{
	uint32_t phase = pg_phase[CYCLES];

	// Rhythm mode
	if constexpr (CYCLES == 12) {
		rm_hh_bits = narrow_cast<uint8_t>(phase >> (2 + 9));
	} else if (CYCLES == 16 && (rhythm & 0x20)) {
		rm_tc_bits = narrow_cast<uint8_t>(phase >> 8);
	}

	if (rhythm & 0x20) {
		auto rm_bit = [&]() {
			bool rm_hh_bit2 = (rm_hh_bits >> (2 - 2)) & 1;
			bool rm_hh_bit3 = (rm_hh_bits >> (3 - 2)) & 1;
			bool rm_hh_bit7 = (rm_hh_bits >> (7 - 2)) & 1;
			bool rm_tc_bit3 = (rm_tc_bits >> (3 + 9 - 8)) & 1;
			bool rm_tc_bit5 = (rm_tc_bits >> (5 + 9 - 8)) & 1;
			return (rm_hh_bit2 ^ rm_hh_bit7)
			     | (rm_hh_bit3 ^ rm_tc_bit5)
			     | (rm_tc_bit3 ^ rm_tc_bit5);
		};
		auto noise_bit = [&]() {
			// see comments in doRhythm()
			return (rm_noise >> (TEST_MODE ? 0 : (CYCLES + 1))) & 1;
		};
		switch (CYCLES) {
		case 12: { // HH
			auto b = rm_bit();
			return (b << 9) | ((b ^ noise_bit()) ? 0xd0 : 0x34);
		}
		case 15: { // SD
			auto rm_hh_bit8 = (rm_hh_bits >> (8 - 2)) & 1;
			return (rm_hh_bit8 << 9) | ((rm_hh_bit8 ^ noise_bit()) << 8);
		}
		case 16: // TC
			return (rm_bit() << 9) | 0x100;
		default:
			break; // suppress warning
		}
	}
	return phase >> 9;
}

template<uint32_t CYCLES> ALWAYS_INLINE uint32_t YM2413::phaseCalcIncrement(const Patch& patch1) const
{
	constexpr uint32_t mcsel = ((CYCLES + 1) / 3) & 1;
	constexpr uint32_t ch = CH_OFFSET[CYCLES];

	uint32_t incr = [&]() {
		// Apply vibrato?
		if (patch1.vib[mcsel]) {
			// note: _must_ be '/ 256' rather than '>> 8' because of
			// round-to-zero for negative values.
			uint32_t freq = fnum[ch] << 1;
			freq += (int(freq) * lfo_vib) / 256;
			return (freq << block[ch]) >> 1;
		} else {
			return uint32_t(p_incr[ch]);
		}
	}();
	return (incr * patch1.multi_t[mcsel]) >> 1;
}

template<uint32_t CYCLES> ALWAYS_INLINE bool YM2413::keyOnEvent() const
{
	bool ismod = ((rhythm & 0x20) && (CYCLES == one_of(12u, 13u)))
	           ? false
	           : (((CYCLES + 4) / 3) & 1);
	return ismod ? eg_dokon[(CYCLES + 3) % 18]
	             : eg_dokon[CYCLES];
}

template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE void YM2413::incrementPhase(uint32_t phase_incr, bool key_on_event)
{
	uint32_t pg_phase_next = ((TEST_MODE && (testMode & 4)) || key_on_event)
	              ? 0
	              : pg_phase[CYCLES];
	pg_phase[CYCLES] = pg_phase_next + phase_incr;
}

template<uint32_t CYCLES> ALWAYS_INLINE void YM2413::channelOutput(std::span<float*, 9 + 5> out, int32_t ch_out)
{
	auto outF = narrow_cast<float>(ch_out);
	switch (CYCLES) {
		case  4: *out[ 0]++ += outF; break;
		case  5: *out[ 1]++ += outF; break;
		case  6: *out[ 2]++ += outF; break;
		case 10: *out[ 3]++ += outF; break;
		case 11: *out[ 4]++ += outF; break;
		case 12: *out[ 5]++ += outF; break;
		case 14: *out[ 9]++ += (rhythm & 0x20) ? 2.0f * outF : 0.0f; break;
		case 15: *out[10]++ += narrow_cast<float>(delay10);
		         delay10     = (rhythm & 0x20) ? 2 * ch_out : 0; break;
		case 16: *out[ 6]++ += narrow_cast<float>(delay6);
			 *out[11]++ += narrow_cast<float>(delay11);
		         delay6      = (rhythm & 0x20) ? 0 : ch_out;
			 delay11     = (rhythm & 0x20) ? 2 * ch_out : 0; break;
		case 17: *out[ 7]++ += narrow_cast<float>(delay7);
			 *out[12]++ += narrow_cast<float>(delay12);
		         delay7      = (rhythm & 0x20) ? 0 : ch_out;
			 delay12     = (rhythm & 0x20) ? 2 * ch_out : 0; break;
		case  0: *out[ 8]++ += (rhythm & 0x20) ? 0.0f : outF;
			 *out[13]++ += (rhythm & 0x20) ? 2.0f * outF : 0.0f; break;
		default:
			break; // suppress warning
	}
}

template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE uint8_t YM2413::envelopeOutput(uint32_t ksltl, int8_t am_t) const
{
	if (TEST_MODE && (testMode & 1)) {
		return 0;
	}
	int32_t level = eg_level[CYCLES] + ksltl + (am_t & lfo_am_out);
	return narrow_cast<uint8_t>(std::min(127, level));
}

template<uint32_t CYCLES, bool TEST_MODE>
ALWAYS_INLINE void YM2413::step(Locals& l)
{
	if constexpr (CYCLES == 11) {
		// the value for 'use_rm_patches' is only meaningful in cycles 11..16
		// and it remains constant during that time.
		l.use_rm_patches = rhythm & 0x20;
	}
	const Patch& patch1 = preparePatch1<CYCLES>(l.use_rm_patches);
	uint32_t ksltl = envelopeKSLTL<CYCLES>(patch1, l.use_rm_patches);
	envelopeTimer1<CYCLES>();
	bool eg_silent = envelopeGenerate1<CYCLES>();
	envelopeTimer2<CYCLES, TEST_MODE>(l.eg_timer_carry);
	envelopeGenerate2<CYCLES>(patch1, l.use_rm_patches);
	bool key_on_event = keyOnEvent<CYCLES>();

	doLFO<CYCLES, TEST_MODE>(l.lfo_am_car);
	doRhythm<CYCLES, TEST_MODE>();
	uint32_t phaseMod = getPhaseMod<CYCLES>(patch1.fb_t);

	constexpr uint32_t mcsel = ((CYCLES + 1) / 3) & 1;
	eg_sl[CYCLES & 1] = patch1.sl[mcsel];
	auto patch2_am_t = patch1.am_t[mcsel];

	uint32_t phase_incr = phaseCalcIncrement<CYCLES>(patch1);
	c_dcm[CYCLES % 3] = patch1.dcm;

	doRegWrite<CYCLES>();
	doIO<CYCLES>();

	doOperator<CYCLES>(l.out, eg_silent);

	uint32_t pg_out = getPhase<CYCLES, TEST_MODE>(l.rm_hh_bits);
	op_phase[CYCLES & 1] = narrow_cast<uint16_t>(phaseMod + pg_out);
	incrementPhase<CYCLES, TEST_MODE>(phase_incr, key_on_event);

	eg_out[CYCLES & 1] = envelopeOutput<CYCLES, TEST_MODE>(ksltl, patch2_am_t);
}

void YM2413::generateChannels(std::span<float*, 9 + 5> out_, uint32_t n)
{
	std::array<float*, 9 + 5> out;
	ranges::copy(out_, out);

	// Loop here (instead of in step18) seems faster. (why?)
	if (test_mode_active) [[unlikely]] {
		repeat(n, [&] { step18<true >(out); });
	} else {
		repeat(n, [&] { step18<false>(out); });
	}
	test_mode_active = testMode;
}

template<bool TEST_MODE>
NEVER_INLINE void YM2413::step18(std::span<float*, 9 + 5> out)
{
	Locals l(out);

	step< 0, TEST_MODE>(l);
	step< 1, TEST_MODE>(l);
	step< 2, TEST_MODE>(l);
	step< 3, TEST_MODE>(l);
	step< 4, TEST_MODE>(l);
	step< 5, TEST_MODE>(l);
	step< 6, TEST_MODE>(l);
	step< 7, TEST_MODE>(l);
	step< 8, TEST_MODE>(l);
	step< 9, TEST_MODE>(l);
	step<10, TEST_MODE>(l);
	step<11, TEST_MODE>(l);
	step<12, TEST_MODE>(l);
	step<13, TEST_MODE>(l);
	step<14, TEST_MODE>(l);
	step<15, TEST_MODE>(l);
	step<16, TEST_MODE>(l);
	step<17, TEST_MODE>(l);

	allowed_offset = std::max<int>(0, allowed_offset - 18); // see writePort()
}

void YM2413::writePort(bool port, uint8_t value, int cycle_offset)
{
	// Hack: detect too-fast access and workaround that.
	//  Ideally we'd just pass this too-fast-access to the NukeYKY code,
	//  because it handles it 'fine' (as in "the same as the real
	//  hardware"). Possibly with a warning.
	//  Though currently in openMSX, when running at 'speed > 100%' we keep
	//  emulate the sound devices at 100% speed, but because CPU runs
	//  faster this can result in artificial too-fast-access. We need to
	//  solve this properly, but for now this hack should suffice.

	if (speedUpHack) [[unlikely]] {
		while (cycle_offset < allowed_offset) [[unlikely]] {
			float d = 0.0f;
			std::array<float*, 9 + 5> dummy = {
				&d, &d, &d, &d, &d, &d, &d, &d, &d,
				&d, &d, &d, &d, &d,
			};
			step18<false>(dummy); // discard result
		}
		// Need 12 cycles (@3.57MHz) wait after address-port access, 84 cycles
		// after data-port access. Divide by 4 to translate to our 18-step
		// timescale.
		allowed_offset = ((port ? 84 : 12) / 4) + cycle_offset;
	}

	writes[cycle_offset] = {port, value};
	if (port && (write_address == 0xf)) {
		test_mode_active = true;
	}

	// only needed for peekReg()
	if (port == 0) {
		latch = value & 63;
	} else {
		regs[latch] = value;
	}
}

void YM2413::pokeReg(uint8_t reg, uint8_t value)
{
	regs[reg] = value;
	if (reg < 0x10) {
		doModeWrite(reg, value);
	} else {
		if (uint8_t ch = reg & 0xf; ch < 9) {
			doRegWrite(reg & 0xf0, ch, value);
		}
	}
}

uint8_t YM2413::peekReg(uint8_t reg) const
{
	return regs[reg & 63];
}

float YM2413::getAmplificationFactor() const
{
	return 1.0f / 256.0f;
}

void YM2413::setSpeed(double speed)
{
	speedUpHack = speed > 1.0;
}

} // namespace YM2413NukeYKT


static constexpr std::initializer_list<enum_string<YM2413NukeYKT::YM2413::EgState>> egStateInfo = {
	{ "attack",  YM2413NukeYKT::YM2413::EgState::attack },
	{ "decay",   YM2413NukeYKT::YM2413::EgState::decay },
	{ "sustain", YM2413NukeYKT::YM2413::EgState::sustain },
	{ "release", YM2413NukeYKT::YM2413::EgState::release }
};
SERIALIZE_ENUM(YM2413NukeYKT::YM2413::EgState, egStateInfo);

namespace YM2413NukeYKT {

template<typename Archive>
void YM2413::Write::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("port", port,
	             "value", value);
}

template<typename Archive>
void YM2413::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("writes", writes,
	             "write_data", write_data,
	             "fm_data", fm_data,
	             "write_address", write_address,
	             "write_fm_cycle", write_fm_cycle,
	             "fast_fm_rewrite", fast_fm_rewrite,
	             "test_mode_active", test_mode_active);

	ar.serialize("eg_timer", eg_timer,
	             "eg_sl", eg_sl,
	             "eg_out", eg_out,
	             "eg_counter_state", eg_counter_state,
	             "eg_timer_shift", eg_timer_shift,
	             "eg_timer_shift_lock", eg_timer_shift_lock,
	             "eg_timer_lock", eg_timer_lock,
	             "eg_state", eg_state,
	             "eg_level", eg_level,
	             "eg_rate", eg_rate,
	             "eg_dokon", eg_dokon,
	             "eg_kon", eg_kon,
	             "eg_off", eg_off,
	             "eg_timer_shift_stop", eg_timer_shift_stop,
	             "pg_phase", pg_phase);

	ar.serialize("op_fb1", op_fb1,
	             "op_fb2", op_fb2,
	             "op_mod", op_mod,
	             "op_phase", op_phase,
	             "lfo_counter", lfo_counter,
	             "lfo_am_counter", lfo_am_counter,
	             "lfo_vib_counter", lfo_vib_counter,
	             "lfo_am_out", lfo_am_out,
	             "lfo_am_step", lfo_am_step,
	             "lfo_am_dir", lfo_am_dir);

	ar.serialize("rm_noise", rm_noise,
	             "rm_tc_bits", rm_tc_bits,
	             "c_dcm", c_dcm,
	             "regs", regs,
	             "latch", latch);

	if constexpr (Archive::IS_LOADER) {
		// restore redundant state
		attackPtr = attack[eg_timer_shift_lock][eg_timer_lock];
		auto idx = releaseIndex[eg_timer_shift_lock][eg_timer_lock][eg_counter_state];
		releasePtr = releaseData[idx];

		lfo_vib = VIB_TAB[lfo_vib_counter];

		delay6 = delay7 = delay10 = delay11 = delay12 = 0;

		// Restore these from register values:
		//   fnum, block, p_ksl, p_incr, p_ksr_freq, sk_on, vol8,
		//   inst, p_inst, rhythm, testMode, patches[0]
		for (auto i : xrange(uint8_t(64))) {
			pokeReg(i, regs[i]);
		}
	}
}

} // namespace YM2413NukeYKT

using YM2413NukeYKT::YM2413;
INSTANTIATE_SERIALIZE_METHODS(YM2413);
REGISTER_POLYMORPHIC_INITIALIZER(YM2413Core, YM2413, "YM2413-NukeYKT");

} // namespace openmsx
