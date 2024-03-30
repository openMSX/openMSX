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
* Heavily modified by Wouter Vermaelen for use in openMSX.
*
* The most important changes are:
* - Adapt to the openMSX API (the 'YM2413Core' base-class).
* - The original NukeYKT code is probably close to how the real hardware is
*   implemented, this is very useful while reverse engineering the YM2413. For
*   example it faithfully implements the YM2413 'pipeline' (how the sound
*   generation suboperations are spread over time). Though, for use in openMSX,
*   we're only interested in the final result. So as long as the generated
*   sound remains 100% identical, we're free to transform the code to a form
*   that's more suited for an efficient software implementation. Of course
*   large parts of this pipeline remain in the transformed code because it does
*   play an important role in for example the exact timing of when register
*   changes take effect. So all transformations have to be tested very
*   carefully.
* - The current code runs (on average) over 3x faster than the original, while
*   still generating identical output. This was measured/tested on a large set
*   of vgm files.
*  - TODO document better what kind of transformations were done.
*      * Emulate 18-steps at-a-time.
*      * Specialize for the common case that testMode==0 (but still have slower
*        fallback code).
*      * Move sub-operations in the pipeline (e.g. to eliminate temporary state)
*        when this doesn't have an observable effect.
*      * Lots of small tweak.
*      * ...
*
* TODO:
* - In openMSX the YM2413 is often silent for large periods of time (e.g. maybe
*   the emulated MSX program doesn't use the YM2413). Can we easily detect an
*   idle YM2413 and then bypass large parts of the emulation?
*/

#ifndef YM2413NUKEYKT_HH
#define YM2413NUKEYKT_HH

#include "YM2413Core.hh"
#include "inline.hh"
#include <array>
#include <span>

namespace openmsx::YM2413NukeYKT {

class YM2413 final : public YM2413Core
{
public:
	YM2413();
	void reset() override;
	void writePort(bool port, uint8_t value, int cycle_offset) override;
	void pokeReg(uint8_t reg, uint8_t value) override;
	[[nodiscard]] uint8_t peekReg(uint8_t reg) const override;
	void generateChannels(std::span<float*, 9 + 5> out, uint32_t n) override;
	[[nodiscard]] float getAmplificationFactor() const override;
	void setSpeed(double speed) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	enum class EgState : uint8_t {
		attack,
		decay,
		sustain,
		release,
	};

private:
	using bool_2    = std::array<bool, 2>;
	using int8_t_2  = std::array<int8_t, 2>;
	using uint8_t_2 = std::array<uint8_t, 2>;
	struct Patch {
		constexpr Patch() = default;
		constexpr Patch(uint8_t tl_, uint8_t dcm_, uint8_t fb_,
		      bool_2 am_, bool_2 vib_, bool_2 et_, bool_2 ksr_, uint8_t_2 multi_,
		      uint8_t_2 ksl_, uint8_t_2 ar_, uint8_t_2 dr_, uint8_t_2 sl_, uint8_t_2 rr_)
			: dcm(dcm_), vib(vib_), et(et_), sl(sl_) {
			setTL(tl_);
			setFB(fb_);
			setAM(0, am_[0]);
			setAM(1, am_[1]);
			setKSR(0, ksr_[0]);
			setKSR(1, ksr_[1]);
			setMulti(0, multi_[0]);
			setMulti(1, multi_[1]);
			setKSL(0, ksl_[0]);
			setKSL(1, ksl_[1]);
			setAR(0, ar_[0]);
			setAR(1, ar_[1]);
			setDR(0, dr_[0]);
			setDR(1, dr_[1]);
			setRR(0, rr_[0]);
			setRR(1, rr_[1]);
		}

		constexpr void setTL(uint8_t tl) { tl2 = 2 * tl; }
		constexpr void setFB(uint8_t fb) { fb_t = fb ? (8 - fb) : 31; }
		constexpr void setAM(int i, bool am) { am_t[i] = am ? -1 : 0; }
		constexpr void setKSR(int i, bool ksr) { ksr_t[i] = ksr ? 0 : 2; }
		constexpr void setMulti(int i, uint8_t multi) {
			constexpr std::array<uint8_t, 16> PG_MULTI = {
				1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
			};
			multi_t[i] = PG_MULTI[multi];
		}
		constexpr void setKSL(int i, uint8_t ksl) { ksl_t[i] = ksl ? (3 - ksl) : 31; }
		constexpr void setAR (int i, uint8_t ar)  { ar4[i] = 4 * ar; }
		constexpr void setDR (int i, uint8_t dr)  { dr4[i] = 4 * dr; }
		constexpr void setRR (int i, uint8_t rr)  { rr4[i] = 4 * rr; }

		uint8_t   tl2 = 0; // multiplied by 2
		uint8_t   dcm = 0;
		uint8_t   fb_t = 31; // 0->31, 1->7, 2->6, .., 6->2, 7->1
		int8_t_2  am_t = {0, 0}; // false->0, true->-1
		bool_2    vib = {false, false};
		bool_2    et  = {false, false};
		uint8_t_2 ksr_t = {2, 2}; // 0->2, 1->0
		uint8_t_2 multi_t = {1, 1}; // transformed via PG_MULTI[]
		uint8_t_2 ksl_t = {31, 31}; // 0->31, 1->2, 2->1, 3->0
		uint8_t_2 ar4 = {0, 0}; // multiplied by 4
		uint8_t_2 dr4 = {0, 0}; // multiplied by 4
		uint8_t_2 sl  = {0, 0};
		uint8_t_2 rr4 = {0, 0}; // multiplied by 4
	};
	struct Locals {
		Locals(std::span<float*, 9 + 5> out_) : out(out_) {}

		std::span<float*, 9 + 5> out;
		uint8_t rm_hh_bits = 0;
		bool use_rm_patches = false;
		bool lfo_am_car = false; // between cycle 17 and 0 'lfo_am_car' is always =0
		bool eg_timer_carry = false;
	};
	struct Write {
		uint8_t port;
		uint8_t value;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};

private:
	template<bool TEST_MODE> NEVER_INLINE void step18(std::span<float*, 9 + 5> out);
	template<uint32_t CYCLES, bool TEST_MODE> ALWAYS_INLINE void step(Locals& l);

	template<uint32_t CYCLES>                 [[nodiscard]] ALWAYS_INLINE uint32_t phaseCalcIncrement(const Patch& patch1) const;
	template<uint32_t CYCLES>                               ALWAYS_INLINE void channelOutput(std::span<float*, 9 + 5> out, int32_t ch_out);
	template<uint32_t CYCLES>                 [[nodiscard]] ALWAYS_INLINE const Patch& preparePatch1(bool use_rm_patches) const;
	template<uint32_t CYCLES, bool TEST_MODE> [[nodiscard]] ALWAYS_INLINE uint32_t getPhase(uint8_t& rm_hh_bits);
	template<uint32_t CYCLES>                 [[nodiscard]] ALWAYS_INLINE bool keyOnEvent() const;
	template<uint32_t CYCLES, bool TEST_MODE>               ALWAYS_INLINE void incrementPhase(uint32_t phase_incr, bool prev_rhythm);
	template<uint32_t CYCLES>                 [[nodiscard]] ALWAYS_INLINE uint32_t getPhaseMod(uint8_t fb_t);
	template<uint32_t CYCLES>                               ALWAYS_INLINE void doOperator(std::span<float*, 9 + 5> out, bool eg_silent);
	template<uint32_t CYCLES, bool TEST_MODE> [[nodiscard]] ALWAYS_INLINE uint8_t envelopeOutput(uint32_t ksltl, int8_t am_t) const;
	template<uint32_t CYCLES>                 [[nodiscard]] ALWAYS_INLINE uint32_t envelopeKSLTL(const Patch& patch1, bool use_rm_patches) const;
	template<uint32_t CYCLES>                               ALWAYS_INLINE void envelopeTimer1();
	template<uint32_t CYCLES, bool TEST_MODE>               ALWAYS_INLINE void envelopeTimer2(bool& eg_timer_carry);
	template<uint32_t CYCLES>                 [[nodiscard]] ALWAYS_INLINE bool envelopeGenerate1();
	template<uint32_t CYCLES>                               ALWAYS_INLINE void envelopeGenerate2(const Patch& patch1, bool use_rm_patches);
	template<uint32_t CYCLES, bool TEST_MODE>               ALWAYS_INLINE void doLFO(bool& lfo_am_car);
	template<uint32_t CYCLES, bool TEST_MODE>               ALWAYS_INLINE void doRhythm();
	template<uint32_t CYCLES>                               ALWAYS_INLINE void doRegWrite();
	template<uint32_t CYCLES>                               ALWAYS_INLINE void doIO();

	NEVER_INLINE void doRegWrite(uint8_t channel);
	             void doRegWrite(uint8_t block, uint8_t channel, uint8_t data);
	NEVER_INLINE void doIO(uint32_t cycles_plus_1, Write& write);

	void doModeWrite(uint8_t address, uint8_t value);
	void changeFnumBlock(uint32_t ch);

private:
	static const std::array<Patch, 15> m_patches;
	static const std::array<Patch,  6> r_patches;

	// IO
	std::array<Write, 18> writes;
	uint8_t write_data;
	uint8_t fm_data;
	uint8_t write_address;
	uint8_t write_fm_cycle;
	bool fast_fm_rewrite;
	bool test_mode_active;

	// Envelope generator
	std::span<const uint8_t, 64> attackPtr;  // redundant: calculate from eg_timer_shift_lock, eg_timer_lock
	std::span<const uint8_t, 64> releasePtr; // redundant: calculate from eg_timer_shift_lock, eg_timer_lock, eg_counter_state
	uint32_t eg_timer;
	std::array<uint8_t, 2> eg_sl;
	std::array<uint8_t, 2> eg_out;
	uint8_t eg_counter_state; // 0..3
	uint8_t eg_timer_shift;
	uint8_t eg_timer_shift_lock;
	uint8_t eg_timer_lock;
	std::array<EgState, 18> eg_state;
	std::array<uint8_t, 18> eg_level;
	std::array<uint8_t, 2> eg_rate;
	std::array<bool, 18> eg_dokon;
	std::array<bool, 2> eg_kon;
	std::array<bool, 2> eg_off;
	bool eg_timer_shift_stop;

	// Phase generator
	std::array<uint32_t, 18> pg_phase;

	// Operator
	std::array<int16_t, 9> op_fb1;
	std::array<int16_t, 9> op_fb2;
	int16_t op_mod;
	std::array<uint16_t, 2> op_phase;

	// LFO
	uint16_t lfo_counter;
	uint16_t lfo_am_counter;
	uint8_t lfo_vib_counter;
	int8_t lfo_vib; // redundant: equal to VIB_TAB[lfo_vib_counter]
	uint8_t lfo_am_out;
	bool lfo_am_step;
	bool lfo_am_dir;

	// Register set
	std::array<uint16_t, 9> fnum;
	std::array<uint8_t, 9> block;
	std::array<uint8_t, 9> p_ksl;      // redundant: calculate from fnum[] and block[]
	std::array<uint16_t, 9> p_incr;    // redundant: calculate from fnum[] and block[]
	std::array<uint8_t, 9> p_ksr_freq; // redundant: calculate from fnum[] and block[]
	std::array<uint8_t, 9> sk_on;
	std::array<uint8_t, 9> vol8; // multiplied by 8
	std::array<uint8_t, 9> inst;
	std::array<const Patch*, 9> p_inst; // redundant: &patches[inst[]]
	uint8_t rhythm;
	uint8_t testMode;
	std::array<Patch, 1 + 15> patches; // user patch (modifiable) + 15 ROM patches
	std::array<uint8_t, 3> c_dcm;

	// Rhythm mode
	uint32_t rm_noise;
	uint8_t rm_tc_bits;

	int delay6;
	int delay7;
	int delay10;
	int delay11;
	int delay12;

	// only used for peekReg();
	std::array<uint8_t, 64> regs;
	uint8_t latch;

	int allowed_offset = 0; // Hack: see comments in writePort()
	bool speedUpHack = false;
};

} // namespace openmsx::NukeYKT

#endif
