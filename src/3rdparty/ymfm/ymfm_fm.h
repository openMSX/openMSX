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
	uint32_t total_level;             // total level * 8 + KSL
	uint32_t block_freq;              // raw block frequency value (used to compute phase_step)
	int32_t detune;                   // detuning value (used to compute phase_step)
	uint32_t multiple;                // multiple value (x.1, used to compute phase_step)
	uint32_t eg_sustain;              // sustain level, shifted up to envelope values
	uint8_t eg_rate[EG_STATES];       // envelope rate, including KSR
	uint8_t eg_shift = 0;             // envelope shift amount
};


// ======================> fm_registers_base

// base class for family-specific register classes; this provides a few
// constants, common defaults, and helpers, but mostly each derived class is
// responsible for defining all commonly-called methods
class fm_registers_base
{
public:
	// this is the size of a full sin waveform
	static constexpr uint32_t WAVEFORM_LENGTH = 0x400;

	//
	// the following constants need to be defined per family:
	//          uint32_t OUTPUTS: The number of outputs exposed (1-4)
	//         uint32_t CHANNELS: The number of channels on the chip
	//        uint32_t OPERATORS: The number of operators on the chip
	//        uint32_t REGISTERS: The number of 8-bit registers allocated
	// uint32_t DEFAULT_PRESCALE: The starting clock prescale
	// uint32_t EG_CLOCK_DIVIDER: The clock divider of the envelope generator
	// uint32_t CSM_TRIGGER_MASK: Mask of channels to trigger in CSM mode
	//         uint32_t REG_MODE: The address of the "mode" register controlling timers
	//     uint8_t STATUS_TIMERA: Status bit to set when timer A fires
	//     uint8_t STATUS_TIMERB: Status bit to set when tiemr B fires
	//       uint8_t STATUS_BUSY: Status bit to set when the chip is busy
	//        uint8_t STATUS_IRQ: Status bit to set when an IRQ is signalled
	//
	// the following constants are uncommon:
	//          bool DYNAMIC_OPS: True if ops/channel can be changed at runtime (OPL3+)
	//       bool EG_HAS_DEPRESS: True if the chip has a DP ("depress"?) envelope stage (OPLL)
	//        bool EG_HAS_REVERB: True if the chip has a faux reverb envelope stage (OPQ/OPZ)
	//           bool EG_HAS_SSG: True if the chip has SSG envelope support (OPN)
	//
	static constexpr bool DYNAMIC_OPS = false;
	static constexpr bool EG_HAS_DEPRESS = false;
	static constexpr bool EG_HAS_REVERB = false;
	static constexpr bool EG_HAS_SSG = false;

	// system-wide register defaults
	uint32_t status_mask() const                     { return 0; } // OPL only
	uint32_t irq_reset() const                       { return 0; } // OPL only

	// per-operator register defaults
	uint32_t op_ssg_eg_enable(uint32_t /*opoffs*/) const { return 0; } // OPN(A) only
	uint32_t op_ssg_eg_mode(uint32_t /*opoffs*/) const   { return 0; } // OPN(A) only

protected:
	// helper to encode four operator numbers into a 32-bit value in the
	// operator maps for each register class
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
};



//*********************************************************
//  CORE ENGINE CLASSES
//*********************************************************

// forward declarations
template<class RegisterType> class fm_engine_base;

// ======================> fm_operator

// fm_operator represents an FM operator (or "slot" in FM parlance), which
// produces an output sine wave modulated by an envelope
template<class RegisterType>
class fm_operator
{
	// "quiet" value, used to optimize when we can skip doing work
	static constexpr uint32_t EG_QUIET = 0x380;

public:
	// constructor
	fm_operator(fm_engine_base<RegisterType> &owner, uint32_t opoffs);

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
		auto state = RegisterType::EG_HAS_REVERB ? EG_REVERB : EG_RELEASE;
		return m_env_state == state && m_env_attenuation >= 0x3ff;
	}

	// Same condition prepare() reports, without refreshing the cache or the key.
	bool audible() const
	{
		auto state = RegisterType::EG_HAS_REVERB ? EG_REVERB : EG_RELEASE;
		return m_env_state != state || m_env_attenuation < EG_QUIET;
	}

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// return the current phase value
	uint32_t phase() const { return m_phase >> 10; }

	// compute operator volume
	int32_t compute_volume(uint32_t phase, uint32_t am_offset) const;

	// key state control
	void keyonoff(uint32_t on, keyon_type type);

	// return a reference to our registers
	RegisterType &regs() const { return m_regs; }

	// simple getters for debugging
	envelope_state debug_eg_state() const { return m_env_state; }
	uint16_t debug_eg_attenuation() const { return m_env_attenuation; }
	uint8_t debug_ssg_inverted() const { return m_ssg_inverted; }
	opdata_cache &debug_cache() { return m_cache; }

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
	uint8_t m_keyon_live;                  // live key on state (bit 0 = direct, bit 1 = rhythm, bit 2 = CSM)
	opdata_cache m_cache;                  // cached values for performance
	RegisterType &m_regs;                  // direct reference to registers
	fm_engine_base<RegisterType> &m_owner; // reference to the owning engine
};


// ======================> fm_channel

// fm_channel represents an FM channel which combines the output of 2 or 4
// operators into a final result
template<class RegisterType>
class fm_channel
{
	using output_data = ymfm_output<RegisterType::OUTPUTS>;

public:
	// constructor
	fm_channel(fm_engine_base<RegisterType> &owner, uint32_t choffs);

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
	void assign(uint32_t index, fm_operator<RegisterType> *op)
	{
		assert(index < m_op.size());
		m_op[index] = op;
		if (op != nullptr)
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
			if (op != nullptr && !op->finished())
				return false;
		return true;
	}

	// Any operator would still produce sound.
	bool audible() const
	{
		for (auto* op : m_op)
			if (op != nullptr && op->audible())
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

	// return a reference to our registers
	RegisterType &regs() const { return m_regs; }

	// simple getters for debugging
	fm_operator<RegisterType> *debug_operator(uint32_t index) const { return m_op[index]; }

private:
	// helper to add values to the outputs based on channel enables
	void add_to_output(uint32_t choffs, output_data &output, int32_t value) const
	{
		// create these constants to appease overzealous compilers checking array
		// bounds in unreachable code (looking at you, clang)
		constexpr int out0_index = 0;
		constexpr int out1_index = 1 % RegisterType::OUTPUTS;
		constexpr int out2_index = 2 % RegisterType::OUTPUTS;
		constexpr int out3_index = 3 % RegisterType::OUTPUTS;

		if (RegisterType::OUTPUTS == 1 || m_regs.ch_output_0(choffs))
			output.data[out0_index] += value;
		if (RegisterType::OUTPUTS >= 2 && m_regs.ch_output_1(choffs))
			output.data[out1_index] += value;
		if (RegisterType::OUTPUTS >= 3 && m_regs.ch_output_2(choffs))
			output.data[out2_index] += value;
		if (RegisterType::OUTPUTS >= 4 && m_regs.ch_output_3(choffs))
			output.data[out3_index] += value;
	}

	// same, with the enables already read into a plan
	void add_to_output(const output_plan &plan, output_data &output, int32_t value) const
	{
		constexpr int out0_index = 0;
		constexpr int out1_index = 1 % RegisterType::OUTPUTS;
		constexpr int out2_index = 2 % RegisterType::OUTPUTS;
		constexpr int out3_index = 3 % RegisterType::OUTPUTS;

		if (plan.output_mask & 1)
			output.data[out0_index] += value;
		if (RegisterType::OUTPUTS >= 2 && (plan.output_mask & 2))
			output.data[out1_index] += value;
		if (RegisterType::OUTPUTS >= 3 && (plan.output_mask & 4))
			output.data[out2_index] += value;
		if (RegisterType::OUTPUTS >= 4 && (plan.output_mask & 8))
			output.data[out3_index] += value;
	}

	// internal state
	uint32_t m_choffs;                     // channel offset in registers
	std::array<int16_t, 2> m_feedback;     // feedback memory for operator 1
	mutable int16_t m_feedback_in;         // next input value for op 1 feedback (set in output)
	std::array<fm_operator<RegisterType> *, 4> m_op; // up to 4 operators
	RegisterType &m_regs;                  // direct reference to registers
	fm_engine_base<RegisterType> &m_owner; // reference to the owning engine
};


// ======================> fm_engine_base

// fm_engine_base represents a set of operators and channels which together
// form a Yamaha FM core; chips that implement other engines (ADPCM, wavetable,
// etc) take this output and combine it with the others externally
template<class RegisterType>
class fm_engine_base : public ymfm_engine_callbacks
{
public:
	// expose some constants from the registers
	static constexpr uint32_t OUTPUTS = RegisterType::OUTPUTS;
	static constexpr uint32_t CHANNELS = RegisterType::CHANNELS;
	static constexpr uint32_t OPERATORS = RegisterType::OPERATORS;

	// also expose status flags for consumers that inject additional bits
	static constexpr uint8_t STATUS_TIMERA = RegisterType::STATUS_TIMERA;
	static constexpr uint8_t STATUS_TIMERB = RegisterType::STATUS_TIMERB;
	static constexpr uint8_t STATUS_BUSY = RegisterType::STATUS_BUSY;
	static constexpr uint8_t STATUS_IRQ = RegisterType::STATUS_IRQ;

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
		return m_status & ~m_regs.status_mask();
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

	// return the owning device
	ymfm_interface &intf() const { return m_intf; }

	// return a reference to our registers
	RegisterType &regs() { return m_regs; }
	const RegisterType &regs() const { return m_regs; }

	// simple getters for debugging
	fm_channel<RegisterType> *debug_channel(uint32_t index) const { return m_channel[index].get(); }
	fm_operator<RegisterType> *debug_operator(uint32_t index) const { return m_operator[index].get(); }

public:
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
	RegisterType m_regs;             // register accessor
	std::array<std::unique_ptr<fm_channel<RegisterType>>, CHANNELS> m_channel; // channel pointers
	std::unique_ptr<fm_operator<RegisterType>> m_operator[OPERATORS]; // operator pointers
};

}

#endif // YMFM_FM_H
