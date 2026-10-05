// BSD 3-Clause License
//
// Copyright (c) 2021, Aaron Giles
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef YMFM_FM_H
#define YMFM_FM_H

#pragma once

#include "ymfm.h"

#include <array>
#include <span>

namespace ymfm
{

//*********************************************************
//  GLOBAL ENUMERATORS
//*********************************************************

// keyon sources; actual keyon is an OR over all of these. Bit 1 was the OPL
// rhythm source, which this build does not have.
enum keyon_type : uint32_t
{
	KEYON_NORMAL = 0,
	KEYON_CSM = 2
};



//*********************************************************
//  CORE IMPLEMENTATION
//*********************************************************

// ======================> opdata_cache

// this class holds data that is computed once at the start of clocking
// and remains static during subsequent sound generation
struct opdata_cache
{
	// set phase_step to this value to recalculate it each sample; needed
	// in the case of PM LFO changes
	static constexpr uint32_t PHASE_STEP_DYNAMIC = 1;

	uint16_t const *waveform;         // base of sine table
	uint32_t phase_step;              // phase step, or PHASE_STEP_DYNAMIC if PM is active
	uint32_t total_level;             // total level * 8
	uint32_t block_freq;              // raw block frequency value (used to compute phase_step)
	int32_t detune;                   // detuning value (used to compute phase_step)
	uint32_t multiple;                // multiple value (x.1, used to compute phase_step)
	uint32_t eg_sustain;              // sustain level, shifted up to envelope values
	uint8_t eg_rate[EG_STATES];       // envelope rate, including KSR
	uint8_t ssg_eg_mode;              // SSG-EG envelope shape (0-7)
	bool ssg_eg_enable;               // true if SSG-EG drives the envelope
	bool lfo_am_enable;               // true if the operator follows the LFO AM offset
};


//*********************************************************
//  REGISTER CLASSES
//*********************************************************

// ======================> opna_registers

//
// OPNA register map:
//
//      System-wide registers:
//           21 xxxxxxxx Test register
//           22 ----x--- LFO enable [OPNA+ only]
//              -----xxx LFO rate [OPNA+ only]
//           24 xxxxxxxx Timer A value (upper 8 bits)
//           25 ------xx Timer A value (lower 2 bits)
//           26 xxxxxxxx Timer B value
//           27 xx------ CSM/Multi-frequency mode for channel #2
//              --x----- Reset timer B
//              ---x---- Reset timer A
//              ----x--- Enable timer B
//              -----x-- Enable timer A
//              ------x- Load timer B
//              -------x Load timer A
//           28 x------- Key on/off operator 4
//              -x------ Key on/off operator 3
//              --x----- Key on/off operator 2
//              ---x---- Key on/off operator 1
//              ------xx Channel select
//
//     Per-channel registers (channel in address bits 0-1)
//     Note that all these apply to address+100 as well on OPNA+
//        A0-A3 xxxxxxxx Frequency number lower 8 bits
//        A4-A7 --xxx--- Block (0-7)
//              -----xxx Frequency number upper 3 bits
//        B0-B3 --xxx--- Feedback level for operator 1 (0-7)
//              -----xxx Operator connection algorithm (0-7)
//        B4-B7 x------- Pan left [OPNA]
//              -x------ Pan right [OPNA]
//              --xx---- LFO AM shift (0-3) [OPNA+ only]
//              -----xxx LFO PM depth (0-7) [OPNA+ only]
//
//     Per-operator registers (channel in address bits 0-1, operator in bits 2-3)
//     Note that all these apply to address+100 as well on OPNA+
//        30-3F -xxx---- Detune value (0-7)
//              ----xxxx Multiple value (0-15)
//        40-4F -xxxxxxx Total level (0-127)
//        50-5F xx------ Key scale rate (0-3)
//              ---xxxxx Attack rate (0-31)
//        60-6F x------- LFO AM enable [OPNA]
//              ---xxxxx Decay rate (0-31)
//        70-7F ---xxxxx Sustain rate (0-31)
//        80-8F xxxx---- Sustain level (0-15)
//              ----xxxx Release rate (0-15)
//        90-9F ----x--- SSG-EG enable
//              -----xxx SSG-EG envelope (0-7)
//
//     Special multi-frequency registers (channel implicitly #2; operator in address bits 0-1)
//        A8-AB xxxxxxxx Frequency number lower 8 bits
//        AC-AF --xxx--- Block (0-7)
//              -----xxx Frequency number upper 3 bits
//
//     Internal (fake) registers:
//        B8-BB --xxxxxx Latched frequency number upper bits (from A4-A7)
//        BC-BF --xxxxxx Latched frequency number upper bits (from AC-AF)
//

class opna_registers
{
public:
	// constants
	static constexpr uint32_t WAVEFORM_LENGTH = 0x400;  // size of a full sin waveform
	static constexpr uint32_t OUTPUTS = 2;
	static constexpr uint32_t CHANNELS = 6;
	static constexpr uint32_t OPERATORS = CHANNELS * 4;
	static constexpr uint32_t REGISTERS = 0x200;
	static constexpr uint32_t REG_MODE = 0x27;
	static constexpr uint32_t DEFAULT_PRESCALE = 6;
	static constexpr uint32_t EG_CLOCK_DIVIDER = 3;
	static constexpr uint32_t CSM_TRIGGER_MASK = 1 << 2;
	static constexpr uint8_t STATUS_TIMERA = 0x01;
	static constexpr uint8_t STATUS_TIMERB = 0x02;
	static constexpr uint8_t STATUS_BUSY = 0x80;

	// constructor
	opna_registers();

	// reset to initial state
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("lfo_counter", m_lfo_counter,
			         "lfo_am", m_lfo_am,
		             "regdata", m_regdata);
	}


	// map channel number to register offset
	static constexpr uint32_t channel_offset(uint32_t chnum)
	{
		assert(chnum < CHANNELS);
		return (chnum % 3) + 0x100 * (chnum / 3);
	}

	// map operator number to register offset
	static constexpr uint32_t operator_offset(uint32_t opnum)
	{
		assert(opnum < OPERATORS);
		return (opnum % 12) + ((opnum % 12) / 3) + 0x100 * (opnum / 12);
	}

	// return an array of operator indices for each channel
	struct operator_mapping { uint32_t chan[CHANNELS]; };
	void operator_map(operator_mapping &dest) const;

	// read a register value
	uint8_t read(uint16_t index) const { return m_regdata[index]; }

	// handle writes to the register array
	bool write(uint16_t index, uint8_t data, uint32_t &chan, uint32_t &opmask);

	// clock the LFO, returning its PM value
	int32_t clock_noise_and_lfo();

	struct lfo_state { uint32_t counter; uint8_t am; };
	lfo_state save_lfo() const { return {m_lfo_counter, m_lfo_am}; }
	void restore_lfo(lfo_state state)
	{
		m_lfo_counter = state.counter;
		m_lfo_am = state.am;
	}

	// return the AM offset from LFO for the given channel
	uint32_t lfo_am_offset(uint32_t choffs) const;

	// caching helpers
	void cache_operator_data(uint32_t choffs, uint32_t opoffs, opdata_cache &cache);

	// compute the phase step, given a PM value
	uint32_t compute_phase_step(uint32_t choffs, uint32_t opoffs, opdata_cache const &cache, int32_t lfo_raw_pm);

	// system-wide registers
	uint32_t lfo_enable() const                 { return byte(0x22, 3, 1); }
	uint32_t lfo_rate() const                   { return byte(0x22, 0, 3); }
	uint32_t timer_a_value() const              { return word(0x24, 0, 8, 0x25, 0, 2); }
	uint32_t timer_b_value() const              { return byte(0x26, 0, 8); }
	uint32_t csm() const                        { return (byte(0x27, 6, 2) == 2); }
	uint32_t multi_freq() const                 { return (byte(0x27, 6, 2) != 0); }
	uint32_t reset_timer_b() const              { return byte(0x27, 5, 1); }
	uint32_t reset_timer_a() const              { return byte(0x27, 4, 1); }
	uint32_t enable_timer_b() const             { return byte(0x27, 3, 1); }
	uint32_t enable_timer_a() const             { return byte(0x27, 2, 1); }
	uint32_t load_timer_b() const               { return byte(0x27, 1, 1); }
	uint32_t load_timer_a() const               { return byte(0x27, 0, 1); }
	uint32_t multi_block_freq(uint32_t num) const    { return word(0xac, 0, 6, 0xa8, 0, 8, num); }

	// per-channel registers
	uint32_t ch_block_freq(uint32_t choffs) const    { return word(0xa4, 0, 6, 0xa0, 0, 8, choffs); }
	uint32_t ch_feedback(uint32_t choffs) const      { return byte(0xb0, 3, 3, choffs); }
	uint32_t ch_algorithm(uint32_t choffs) const     { return byte(0xb0, 0, 3, choffs); }
	uint32_t ch_output_any(uint32_t choffs) const    { return byte(0xb4, 6, 2, choffs); }
	uint32_t ch_output_0(uint32_t choffs) const      { return byte(0xb4, 7, 1, choffs); }
	uint32_t ch_output_1(uint32_t choffs) const      { return byte(0xb4, 6, 1, choffs); }
	uint32_t ch_lfo_am_sens(uint32_t choffs) const   { return byte(0xb4, 4, 2, choffs); }
	uint32_t ch_lfo_pm_sens(uint32_t choffs) const   { return byte(0xb4, 0, 3, choffs); }

	// per-operator registers
	uint32_t op_detune(uint32_t opoffs) const        { return byte(0x30, 4, 3, opoffs); }
	uint32_t op_multiple(uint32_t opoffs) const      { return byte(0x30, 0, 4, opoffs); }
	uint32_t op_total_level(uint32_t opoffs) const   { return byte(0x40, 0, 7, opoffs); }
	uint32_t op_ksr(uint32_t opoffs) const           { return byte(0x50, 6, 2, opoffs); }
	uint32_t op_attack_rate(uint32_t opoffs) const   { return byte(0x50, 0, 5, opoffs); }
	uint32_t op_decay_rate(uint32_t opoffs) const    { return byte(0x60, 0, 5, opoffs); }
	uint32_t op_lfo_am_enable(uint32_t opoffs) const { return byte(0x60, 7, 1, opoffs); }
	uint32_t op_sustain_rate(uint32_t opoffs) const  { return byte(0x70, 0, 5, opoffs); }
	uint32_t op_sustain_level(uint32_t opoffs) const { return byte(0x80, 4, 4, opoffs); }
	uint32_t op_release_rate(uint32_t opoffs) const  { return byte(0x80, 0, 4, opoffs); }
	uint32_t op_ssg_eg_enable(uint32_t opoffs) const { return byte(0x90, 3, 1, opoffs); }
	uint32_t op_ssg_eg_mode(uint32_t opoffs) const   { return byte(0x90, 0, 3, opoffs); }

protected:
	// helper to encode four operator numbers into a 32-bit value in the
	// operator map
	static constexpr uint32_t operator_list(uint8_t o1 = 0xff, uint8_t o2 = 0xff, uint8_t o3 = 0xff, uint8_t o4 = 0xff)
	{
		return o1 | (o2 << 8) | (o3 << 16) | (o4 << 24);
	}

	// helper to apply KSR to the raw ADSR rate, ignoring ksr if the
	// raw value is 0, and clamping to 63
	static constexpr uint32_t effective_rate(uint32_t rawrate, uint32_t ksr)
	{
		return (rawrate == 0) ? 0 : std::min<uint32_t>(rawrate + ksr, 63);
	}

	// return a bitfield extracted from a byte
	uint32_t byte(uint32_t offset, uint32_t start, uint32_t count, uint32_t extra_offset = 0) const
	{
		return bitfield(m_regdata[offset + extra_offset], start, count);
	}

	// return a bitfield extracted from a pair of bytes, MSBs listed first
	uint32_t word(uint32_t offset1, uint32_t start1, uint32_t count1, uint32_t offset2, uint32_t start2, uint32_t count2, uint32_t extra_offset = 0) const
	{
		return (byte(offset1, start1, count1, extra_offset) << count2) | byte(offset2, start2, count2, extra_offset);
	}

	// internal state
	uint32_t m_lfo_counter;               // LFO counter
	uint8_t m_lfo_am;                     // current LFO AM value
	std::array<uint8_t, REGISTERS> m_regdata;    // register data
	std::array<uint16_t, WAVEFORM_LENGTH> m_waveform; // the single waveform
};



//*********************************************************
//  CORE ENGINE CLASSES
//*********************************************************

// ======================> fm_operator

// fm_operator represents an FM operator (or "slot" in FM parlance), which
// produces an output sine wave modulated by an envelope
class fm_operator
{
	// "quiet" value, used to optimize when we can skip doing work
	static constexpr uint32_t EG_QUIET = 0x380;

public:
	// constructor
	fm_operator(opna_registers &regs, uint32_t opoffs);

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("phase",           m_phase,
		             "env_attenuation", m_env_attenuation,
		             "env_state",       m_env_state,
		             "ssg_inverted",    m_ssg_inverted,
		             "key_state",       m_key_state,
		             "keyon_live",      m_keyon_live);
	}

	// reset the operator state
	void reset();

	// return the operator/channel offset
	uint32_t opoffs() const { return m_opoffs; }
	uint32_t choffs() const { return m_choffs; }

	// set the current channel
	void set_choffs(uint32_t choffs) { m_choffs = choffs; }

	// prepare prior to clocking
	bool prepare();

	// Release has reached maximum attenuation. A later key-on restarts phase.
	bool finished() const
	{
		return m_env_state == EG_RELEASE && m_env_attenuation >= 0x3ff;
	}

	// Same condition prepare() reports, without refreshing the cache or the key.
	bool audible() const
	{
		return m_env_state != EG_RELEASE || m_env_attenuation < EG_QUIET;
	}

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// return the current phase value
	uint32_t phase() const { return m_phase >> 10; }

	// compute operator volume
	int32_t compute_volume(uint32_t phase, uint32_t am_offset) const;

	// key state control
	void keyonoff(uint32_t on, keyon_type type);

private:
	// start the attack phase
	void start_attack(bool is_restart = false);

	// start the release phase
	void start_release();

	// clock phases
	void clock_keystate(uint32_t keystate);
	void clock_ssg_eg_state();
	void clock_envelope(uint32_t env_counter);
	void clock_phase(int32_t lfo_raw_pm);

	// return effective attenuation of the envelope
	uint32_t envelope_attenuation(uint32_t am_offset) const;

	// internal state
	uint32_t m_choffs;                     // channel offset in registers
	uint32_t m_opoffs;                     // operator offset in registers
	uint32_t m_phase;                      // current phase value (10.10 format)
	uint16_t m_env_attenuation;            // computed envelope attenuation (4.6 format)
	envelope_state m_env_state;            // current envelope state
	uint8_t m_ssg_inverted;                // non-zero if the output should be inverted (bit 0)
	uint8_t m_key_state;                   // current key state: on or off (bit 0)
	uint8_t m_keyon_live;                  // live key on state (bit 0 = direct, bit 2 = CSM)
	opdata_cache m_cache;                  // cached values for performance
	opna_registers &m_regs;                // direct reference to registers
};


// ======================> fm_channel

// fm_channel represents an FM channel which combines the output of 2 or 4
// operators into a final result
class fm_channel
{
	using output_data = ymfm_output<opna_registers::OUTPUTS>;

public:
	// constructor
	fm_channel(opna_registers &regs, uint32_t choffs);

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("feedback",    m_feedback,
		             "feedback_in", m_feedback_in);
	}

	// reset the channel state
	void reset();

	// return the channel offset
	uint32_t choffs() const { return m_choffs; }

	// assign operators
	void assign(uint32_t index, fm_operator *op)
	{
		assert(index < m_op.size());
		assert(op != nullptr);
		m_op[index] = op;
		op->set_choffs(m_choffs);
	}

	// signal key on/off to our operators
	void keyonoff(uint32_t states, keyon_type type, uint32_t chnum);

	// prepare prior to clocking
	bool prepare();

	// Every operator has finished its release.
	bool finished() const
	{
		for (auto* op : m_op)
			if (!op->finished())
				return false;
		return true;
	}

	// Any operator would still produce sound.
	bool audible() const
	{
		for (auto* op : m_op)
			if (op->audible())
				return true;
		return false;
	}

	// Feedback shift for a skipped mix. After two clocks both slots hold the
	// last written sample, and further clocks leave them there.
	void quiesce_feedback(unsigned num)
	{
		if (num >= 2) {
			m_feedback[0] = m_feedback[1] = m_feedback_in;
		} else if (num == 1) {
			m_feedback[0] = m_feedback[1];
			m_feedback[1] = m_feedback_in;
		}
	}

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// Register fields that output_4op() reads. A register write ends the
	// current buffer, so these hold for every sample of one generate().
	struct output_plan
	{
		uint16_t algorithm_ops;  // operator routing for the algorithm
		uint8_t feedback;        // operator 1 self-feedback, 0 means none
		uint8_t output_mask;     // one bit per output this channel feeds
		bool output_any;         // any output enabled
	};

	// Read those fields once, before the sample loop.
	output_plan make_output_plan() const;

	// 4-operator output handler
	void output_4op(output_data &output, const output_plan &plan, uint32_t am_offset,
	                uint32_t rshift, int32_t clipmax) const;

private:
	// helper to add a value to the left/right outputs the plan enables
	void add_to_output(const output_plan &plan, output_data &output, int32_t value) const
	{
		if (plan.output_mask & 1)
			output.data[0] += value;
		if (plan.output_mask & 2)
			output.data[1] += value;
	}

	// internal state
	uint32_t m_choffs;                     // channel offset in registers
	std::array<int16_t, 2> m_feedback;     // feedback memory for operator 1
	mutable int16_t m_feedback_in;         // next input value for op 1 feedback (set in output)
	std::array<fm_operator *, 4> m_op;     // the four operators of this channel
	opna_registers &m_regs;                // direct reference to registers
};


// ======================> fm_engine_base

// fm_engine_base represents a set of operators and channels which together
// form a Yamaha FM core; chips that implement other engines (ADPCM, wavetable,
// etc) take this output and combine it with the others externally
class fm_engine_base : public ymfm_engine_callbacks
{
public:
	// expose some constants from the registers
	static constexpr uint32_t OUTPUTS = opna_registers::OUTPUTS;
	static constexpr uint32_t CHANNELS = opna_registers::CHANNELS;
	static constexpr uint32_t OPERATORS = opna_registers::OPERATORS;

	// also expose status flags for consumers that inject additional bits
	static constexpr uint8_t STATUS_TIMERA = opna_registers::STATUS_TIMERA;
	static constexpr uint8_t STATUS_TIMERB = opna_registers::STATUS_TIMERB;
	static constexpr uint8_t STATUS_BUSY = opna_registers::STATUS_BUSY;

	// expose the correct output class
	using output_data = ymfm_output<OUTPUTS>;

	// constructor
	fm_engine_base(ymfm_interface &intf);

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		// save our data
		ar.serialize("env_counter",    m_env_counter,
		             "status",         m_status,
		             "clock_prescale", m_clock_prescale,
		             "irq_mask",       m_irq_mask,
		             "irq_state",      m_irq_state,
		             "timer_running",  m_timer_running,
		             "total_clocks",   m_total_clocks,
		             "regs",           m_regs);
		for (uint32_t chnum = 0; chnum < CHANNELS; ++chnum) {
			ar.serialize("channel", *m_channel[chnum]);
		}
		for (uint32_t opnum = 0; opnum < OPERATORS; ++opnum) {
			ar.serialize("operator", *m_operator[opnum]);
		}
		// Operator caches are not saved. The next generate() rebuilds them.
		m_modified = true;
	}

	// reset the overall state
	void reset();

	// Envelope counter before the next generate(). ADPCM-A replays the same
	// step to decide which samples it clocks.
	uint32_t envelope_counter() const { return m_env_counter; }

	// Whole buffer, one channel at a time. prepare() runs when a register or
	// key changed; otherwise the current envelope state is enough. A channel
	// whose operators have all reached release attenuation 0x3ff is not
	// clocked. The envelope counter still advances for ADPCM-A. Callers pass
	// a real buffer per channel. Channels outside chanmask, and channels that
	// are quiet, are nulled.
	void generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t chanmask, uint32_t rshift, int32_t clipmax);

	// write to the OPN registers
	void write(uint16_t regnum, uint8_t data);

	// return the current status
	uint8_t status() const;

	// set/reset bits in the status register, updating the IRQ status
	uint8_t set_reset_status(uint8_t set, uint8_t reset)
	{
		m_status = (m_status | set) & ~(reset | STATUS_BUSY);
		m_intf.ymfm_sync_check_interrupts();
		return m_status;
	}

	// set the IRQ mask
	void set_irq_mask(uint8_t mask) { m_irq_mask = mask; m_intf.ymfm_sync_check_interrupts(); }

	// return the current clock prescale
	uint32_t clock_prescale() const { return m_clock_prescale; }

	// set prescale factor (2/3/6)
	void set_clock_prescale(uint32_t prescale) { m_clock_prescale = prescale; }

	// compute sample rate
	uint32_t sample_rate(uint32_t baseclock) const
	{
		return baseclock / (m_clock_prescale * OPERATORS);
	}

	// return a reference to our registers
	opna_registers &regs() { return m_regs; }
	const opna_registers &regs() const { return m_regs; }

	// timer callback; called by the interface when a timer fires
	virtual void engine_timer_expired(uint32_t tnum) override;

	// check interrupts; called by the interface after synchronization
	virtual void engine_check_interrupts() override;

	// mode register write; called by the interface after synchronization
	virtual void engine_mode_write(uint8_t data) override;

protected:
	// assign the current set of operators to channels
	void assign_operators();

	// update the state of the given timer
	void update_timer(uint32_t which, uint32_t enable, int32_t delta_clocks);

	// internal state
	ymfm_interface &m_intf;          // reference to the system interface
	uint32_t m_env_counter;          // envelope counter; low 2 bits are sub-counter
	uint8_t m_status;                // current status register
	uint8_t m_clock_prescale;        // prescale factor (2/3/6)
	uint8_t m_irq_mask;              // mask of which bits signal IRQs
	uint8_t m_irq_state;             // current IRQ state
	std::array<uint8_t, 2> m_timer_running;      // current timer running state
	uint8_t m_total_clocks;          // low 8 bits of the total number of clocks processed
	bool m_modified;                 // register or key changed since the last generate()
	opna_registers m_regs;           // register accessor
	std::array<std::unique_ptr<fm_channel>, CHANNELS> m_channel; // channel pointers
	std::unique_ptr<fm_operator> m_operator[OPERATORS];          // operator pointers
};

}

#endif // YMFM_FM_H
