#ifndef YM2608_HH
#define YM2608_HH

// YM2608 OPNA core. FM and ADPCM engines started as YMFM by Aaron Giles
// (BSD-3-Clause); see src/3rdparty/ymfm/LICENSE. The engines are now
// owned openMSX code specialized for this chip.

#include "AY8910.hh"
#include "ResampledSoundDevice.hh"

#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "Ram.hh"
#include "SimpleDebuggable.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <string_view>

namespace openmsx {

constexpr uint32_t bitfield(uint32_t value, int start, int length = 1)
{
	return (value >> start) & ((1 << length) - 1);
}

enum envelope_state : uint8_t
{
	EG_ATTACK,
	EG_DECAY,
	EG_SUSTAIN,
	EG_RELEASE,
	EG_STATES,
};

// keyon sources; actual keyon is an OR over all of these.
enum keyon_type : uint8_t
{
	KEYON_NORMAL,
	KEYON_CSM,
};


// This class holds data that is computed once at the start of clocking
// and remains static during subsequent sound generation
struct opdata_cache
{
	// set phase_step to this value to recalculate it each sample; needed
	// in the case of PM LFO changes
	static constexpr uint32_t PHASE_STEP_DYNAMIC = 1;

	uint32_t phase_step;                    // phase step, or PHASE_STEP_DYNAMIC if PM is active
	uint16_t total_level;                   // total level * 8
	uint16_t block_freq;                    // raw block frequency value (used to compute phase_step)
	uint16_t eg_sustain;                    // sustain level, shifted up to envelope values
	int8_t detune;                          // detuning value (used to compute phase_step)
	uint8_t multiple;                       // multiple value (x.1, used to compute phase_step)
	std::array<uint8_t, EG_STATES> eg_rate; // envelope rate, including KSR
	uint8_t lfo_pm_sens;                    // LFO PM sensitivity (0-7)
	uint8_t ssg_eg_mode;                    // SSG-EG envelope shape (0-7)
	bool ssg_eg_enable;                     // true if SSG-EG drives the envelope
	bool lfo_am_enable;                     // true if the operator follows the LFO AM offset
};


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
	static constexpr unsigned CHANNELS = 6;
	static constexpr unsigned OPERATORS = CHANNELS * 4;
	static constexpr uint32_t REGISTERS = 0x200;
	static constexpr uint32_t REG_MODE = 0x27;

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// map channel number to register offset
	static constexpr unsigned channel_offset(unsigned chNum)
	{
		assert(chNum < CHANNELS);
		return (chNum % 3) + 0x100 * (chNum / 3);
	}

	// map operator number to register offset
	static constexpr unsigned operator_offset(unsigned opNum)
	{
		assert(opNum < OPERATORS);
		return (opNum % 12) + ((opNum % 12) / 3) + 0x100 * (opNum / 12);
	}

	// Operator number for each channel's four slots; fixed on OPNA.
	//
	// The chip numbers a channel's operators carrier 1, carrier 2, modulator 1,
	// modulator 2, while wiring up the connections wants carrier 1, modulator 1,
	// carrier 2, modulator 2, so each row below is listed in that second order.
	static constexpr uint8_t OPERATOR_MAP[CHANNELS][4] =
	{
		{  0,  6,  3,  9 },  // Channel 0 operators
		{  1,  7,  4, 10 },  // Channel 1 operators
		{  2,  8,  5, 11 },  // Channel 2 operators
		{ 12, 18, 15, 21 },  // Channel 3 operators
		{ 13, 19, 16, 22 },  // Channel 4 operators
		{ 14, 20, 17, 23 },  // Channel 5 operators
	};

	// read a register value
	[[nodiscard]] uint8_t read(uint16_t index) const { return m_regData[index]; }

	// handle writes to the register array
	bool write(uint16_t index, uint8_t data, unsigned& chan, unsigned& opMask);

	// A disabled LFO holds its counter at 0. Position 0 gives an AM value of
	// 0x3f, and music depends on that added attenuation: MegaDrive Venom plays
	// notes with the LFO globally disabled while enabling AM on the operators.
	void hold_disabled_lfo();

	// Divider count for the current LFO rate; constant for a whole buffer.
	[[nodiscard]] uint32_t lfo_max_count() const;

	// clock an enabled LFO, returning its PM value
	int32_t clock_lfo(uint32_t max_count);

	struct lfo_state { uint32_t counter; uint8_t am; };
	[[nodiscard]] lfo_state save_lfo() const;
	void restore_lfo(lfo_state state);

	// AM shift for a channel's sensitivity; constant for a whole buffer
	[[nodiscard]] uint32_t lfo_am_shift(unsigned chOffs) const;

	// return the AM offset from LFO for that shift
	[[nodiscard]] uint32_t lfo_am_offset(uint32_t am_shift) const;

	// caching helpers
	void cache_operator_data(unsigned chOffs, unsigned opOffs, opdata_cache& cache);

	// compute the phase step, given a PM value
	static uint32_t compute_phase_step(const opdata_cache& cache, int32_t lfo_raw_pm);

	// system-wide registers
	[[nodiscard]] bool lfo_enable() const                       { return byte(0x22, 3, 1) != 0; }
	[[nodiscard]] uint32_t lfo_rate() const                     { return byte(0x22, 0, 3); }
	[[nodiscard]] uint32_t timer_a_value() const                { return word(0x24, 0, 8, 0x25, 0, 2); }
	[[nodiscard]] uint32_t timer_b_value() const                { return byte(0x26, 0, 8); }
	[[nodiscard]] bool csm() const                              { return byte(0x27, 6, 2) == 2; }
	[[nodiscard]] bool multi_freq() const                       { return byte(0x27, 6, 2) != 0; }
	[[nodiscard]] bool reset_timer_b() const                    { return byte(0x27, 5, 1) != 0; }
	[[nodiscard]] bool reset_timer_a() const                    { return byte(0x27, 4, 1) != 0; }
	[[nodiscard]] bool enable_timer_b() const                   { return byte(0x27, 3, 1) != 0; }
	[[nodiscard]] bool enable_timer_a() const                   { return byte(0x27, 2, 1) != 0; }
	[[nodiscard]] bool load_timer_b() const                     { return byte(0x27, 1, 1) != 0; }
	[[nodiscard]] bool load_timer_a() const                     { return byte(0x27, 0, 1) != 0; }
	[[nodiscard]] uint32_t multi_block_freq(unsigned num) const { return word(0xac, 0, 6, 0xa8, 0, 8, num); }

	// per-channel registers
	[[nodiscard]] uint32_t ch_block_freq(unsigned chOffs) const    { return word(0xa4, 0, 6, 0xa0, 0, 8, chOffs); }
	[[nodiscard]] uint32_t ch_feedback(unsigned chOffs) const      { return byte(0xb0, 3, 3, chOffs); }
	[[nodiscard]] uint32_t ch_algorithm(unsigned chOffs) const     { return byte(0xb0, 0, 3, chOffs); }
	[[nodiscard]] bool ch_output_0(unsigned chOffs) const          { return byte(0xb4, 7, 1, chOffs) != 0; }
	[[nodiscard]] bool ch_output_1(unsigned chOffs) const          { return byte(0xb4, 6, 1, chOffs) != 0; }
	[[nodiscard]] uint32_t ch_lfo_am_sens(unsigned chOffs) const   { return byte(0xb4, 4, 2, chOffs); }
	[[nodiscard]] uint32_t ch_lfo_pm_sens(unsigned chOffs) const   { return byte(0xb4, 0, 3, chOffs); }

	// per-operator registers
	[[nodiscard]] uint32_t op_detune(unsigned opOffs) const        { return byte(0x30, 4, 3, opOffs); }
	[[nodiscard]] uint32_t op_multiple(unsigned opOffs) const      { return byte(0x30, 0, 4, opOffs); }
	[[nodiscard]] uint32_t op_total_level(unsigned opOffs) const   { return byte(0x40, 0, 7, opOffs); }
	[[nodiscard]] uint32_t op_ksr(unsigned opOffs) const           { return byte(0x50, 6, 2, opOffs); }
	[[nodiscard]] uint32_t op_attack_rate(unsigned opOffs) const   { return byte(0x50, 0, 5, opOffs); }
	[[nodiscard]] uint32_t op_decay_rate(unsigned opOffs) const    { return byte(0x60, 0, 5, opOffs); }
	[[nodiscard]] bool op_lfo_am_enable(unsigned opOffs) const     { return byte(0x60, 7, 1, opOffs) != 0; }
	[[nodiscard]] uint32_t op_sustain_rate(unsigned opOffs) const  { return byte(0x70, 0, 5, opOffs); }
	[[nodiscard]] uint32_t op_sustain_level(unsigned opOffs) const { return byte(0x80, 4, 4, opOffs); }
	[[nodiscard]] uint32_t op_release_rate(unsigned opOffs) const  { return byte(0x80, 0, 4, opOffs); }
	[[nodiscard]] bool op_ssg_eg_enable(unsigned opOffs) const     { return byte(0x90, 3, 1, opOffs) != 0; }
	[[nodiscard]] uint32_t op_ssg_eg_mode(unsigned opOffs) const   { return byte(0x90, 0, 3, opOffs); }

private:
	// return a bitfield extracted from a byte
	[[nodiscard]] uint32_t byte(uint32_t offset, uint32_t start, uint32_t count, uint32_t extra_offset = 0) const
	{
		return bitfield(m_regData[offset + extra_offset], start, count);
	}

	// return a bitfield extracted from a pair of bytes, MSBs listed first
	[[nodiscard]] uint32_t word(uint32_t offset1, uint32_t start1, uint32_t count1, uint32_t offset2, uint32_t start2, uint32_t count2, uint32_t extra_offset = 0) const
	{
		return (byte(offset1, start1, count1, extra_offset) << count2) | byte(offset2, start2, count2, extra_offset);
	}

	uint32_t m_lfo_counter = 0;               // LFO counter
	uint8_t m_lfo_am = 0;                     // current LFO AM value
	std::array<uint8_t, REGISTERS> m_regData; // register data
};


// fm_operator represents an FM operator (or "slot" in FM parlance), which
// produces an output sine wave modulated by an envelope
class fm_operator
{
	// "quiet" value, used to optimize when we can skip doing work
	static constexpr uint32_t EG_QUIET = 0x380;

public:
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void reset();

	// prepare prior to clocking; the registers are read only here
	bool prepare(opna_registers& regs, unsigned chOffs, unsigned opOffs);

	// Release has reached maximum attenuation. A later key-on restarts phase.
	[[nodiscard]] bool finished() const;

	// Same condition prepare() reports, without refreshing the cache or the key.
	[[nodiscard]] bool audible() const;

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// return the current phase value
	[[nodiscard]] uint32_t phase() const { return m_phase >> 10; }

	// 14-bit signed volume, given phase modulation and an AM LFO offset
	[[nodiscard]] int32_t compute_volume(uint32_t phase, uint32_t am_offset) const;

	// key state control
	void keyOnOff(bool on, keyon_type type);

private:
	void start_attack(bool is_restart = false);
	void start_release();

	void clock_keystate(bool keyState);
	void clock_ssg_eg_state();
	void clock_envelope(uint32_t env_counter);
	void clock_phase(int32_t lfo_raw_pm);

	// return effective attenuation of the envelope
	[[nodiscard]] uint32_t envelope_attenuation(uint32_t am_offset) const;

private:
	uint32_t m_phase = 0;                    // current phase value (10.10 format)
	uint16_t m_env_attenuation = 0x3ff;      // computed envelope attenuation (4.6 format)
	envelope_state m_env_state = EG_RELEASE; // current envelope state
	bool m_ssg_inverted = false;             // true if the output should be inverted
	bool m_key_state = false;                // current key state
	uint8_t m_keyon_live = 0;                // live key on state (bit 0 = direct, bit 2 = CSM)
	opdata_cache m_cache{};                  // cached values for performance
};


// fm_channel represents an FM channel which combines the output of 2 or 4
// operators into a final result
class fm_channel
{
	// OPNA scales each carrier down by one bit before summing them
	static constexpr uint32_t OUTPUT_SHIFT = 1;

public:
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void reset();

	// signal key on/off to our operators
	void keyOnOff(uint32_t states, keyon_type type);

	// prepare prior to clocking; the registers are read only here
	bool prepare(opna_registers& regs, unsigned chNum);

	// Every operator has finished its release.
	[[nodiscard]] bool finished() const;

	// Any operator would still produce sound.
	[[nodiscard]] bool audible() const;

	// Feedback shift for a skipped mix. After two clocks both slots hold the
	// last written sample, and further clocks leave them there.
	void quiesce_feedback(unsigned num);

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// Register fields that output_4op() reads. A register write ends the
	// current buffer, so these hold for every sample of one generate().
	struct output_plan
	{
		uint8_t op2in;        // opout[] index that modulates operator 2
		uint8_t op3in;        // opout[] index that modulates operator 3
		uint8_t op4in;        // opout[] index that modulates operator 4
		uint8_t carrier_mask; // bit 0 = op1, bit 1 = op2, bit 2 = op3 in the sum
		uint8_t feedback;     // operator 1 self-feedback, 0 means none
		uint8_t output_mask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop.
	[[nodiscard]] output_plan make_output_plan(const opna_registers& regs, unsigned chNum) const;

	// 4-operator output handler; the caller routes the result to the outputs
	// the plan enables
	int32_t output_4op(const output_plan& plan, uint32_t am_offset);

private:
	std::array<int16_t, 2> m_feedback{};   // feedback memory for operator 1
	int16_t m_feedback_in = 0;             // next input value for op 1 feedback (set in output_4op)
	std::array<fm_operator, 4> m_op;
};


// fm_engine represents a set of operators and channels which together
// form a Yamaha FM core; chips that implement other engines (ADPCM, wavetable,
// etc) take this output and combine it with the others externally
class fm_engine
{
public:
	static constexpr unsigned CHANNELS = opna_registers::CHANNELS;
	static constexpr unsigned OPERATORS = opna_registers::OPERATORS;
	static constexpr uint8_t STATUS_TIMER_A = 0x01;
	static constexpr uint8_t STATUS_TIMER_B = 0x02;

	fm_engine(MSXMotherBoard& motherboard, std::string_view name);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void reset(EmuTime time);

	// Envelope counter before the next generate(). ADPCM-A replays the same
	// step to decide which samples it clocks.
	[[nodiscard]] uint32_t envelope_counter() const { return m_env_counter; }

	// Whole buffer, one channel at a time. prepare() runs when a register or
	// key changed; otherwise the current envelope state is enough. A channel
	// whose operators have all reached release attenuation 0x3ff is not
	// clocked. The envelope counter still advances for ADPCM-A. Callers pass
	// a real buffer per channel. Channels outside chanMask, and channels that
	// are quiet, are nulled.
	void generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t chanMask);

	// write to the OPN registers
	void write(uint16_t regNum, uint8_t data, EmuTime time);

	// return the current status
	uint8_t status() const;

	// set/reset bits in the status register, updating the IRQ status
	uint8_t set_reset_status(uint8_t set, uint8_t reset);

	// set the IRQ mask
	void set_irq_mask(uint8_t mask) { m_irq_mask = mask; check_interrupts(); }

	// return the current clock prescale
	[[nodiscard]] uint32_t clock_prescale() const { return m_clock_prescale; }

	// set prescale factor (2/3/6)
	void set_clock_prescale(uint32_t prescale) { m_clock_prescale = prescale; }

	[[nodiscard]] EmuTime getCurrentTime() const { return timers[0].getCurrentTime(); }

	[[nodiscard]]       auto& regs()       { return m_regs; }
	[[nodiscard]] const auto& regs() const { return m_regs; }

private:
	class Timer final : public Schedulable {
	public:
		Timer(Scheduler& scheduler_, uint8_t index);
		void cancel() { removeSyncPoints(); }
		void schedule(EmuTime time)
		{
			cancel();
			setSyncPoint(time);
		}

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		void executeUntil(EmuTime time) override;

	private:
		uint8_t index;
	};

	void engine_timer_expired(unsigned tNum, EmuTime time);
	void check_interrupts();
	void mode_write(uint8_t data, EmuTime time);
	void update_timer(unsigned which, bool enable, int32_t delta_clocks, EmuTime time);
	void scheduleTimer(unsigned timer, int32_t duration, EmuTime time);

private:
	IRQHelper irq;
	std::array<Timer, 2> timers;
	uint32_t m_env_counter;          // envelope counter; low 2 bits are sub-counter
	uint8_t m_status;                // current status register
	uint8_t m_clock_prescale;        // prescale factor (2/3/6)
	uint8_t m_irq_mask;              // mask of which bits signal IRQs
	std::array<bool, 2> m_timer_running;         // current timer running state
	uint8_t m_total_clocks;          // low 8 bits of the total number of clocks processed
	bool m_modified;                 // register or key changed since the last generate()
	opna_registers m_regs;           // register accessor
	std::array<fm_channel, CHANNELS> m_channel;
};


// ADPCM-A register map:
//
//      System-wide registers:
//           00 x------- Dump (disable=1) or keyon (0) control
//              --xxxxxx Mask of channels to dump or keyon
//           01 --xxxxxx Total level
//           02 xxxxxxxx Test register
//        08-0D x------- Pan left
//              -x------ Pan right
//              ---xxxxx Instrument level
//        10-15 xxxxxxxx Start address (low)
//        18-1D xxxxxxxx Start address (high)
//        20-25 xxxxxxxx End address (low)
//        28-2D xxxxxxxx End address (high)
//
class adpcm_a_registers
{
public:
	static constexpr unsigned CHANNELS = 6;
	static constexpr uint32_t REGISTERS = 0x30;

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	[[nodiscard]] uint8_t read(uint32_t index) const { return m_regdata[index]; }
	void write(uint32_t index, uint8_t data) { m_regdata[index] = data; }

	// system-wide registers
	[[nodiscard]] uint32_t total_level() const                        { return bitfield(m_regdata[0x01], 0, 6); }

	// per-channel registers
	[[nodiscard]] bool ch_pan_left(unsigned chOffs) const             { return bitfield(m_regdata[chOffs + 0x08], 7) != 0; }
	[[nodiscard]] bool ch_pan_right(unsigned chOffs) const            { return bitfield(m_regdata[chOffs + 0x08], 6) != 0; }
	[[nodiscard]] uint32_t ch_instrument_level(unsigned chOffs) const { return bitfield(m_regdata[chOffs + 0x08], 0, 5); }
	[[nodiscard]] uint32_t ch_start(unsigned chOffs) const            { return m_regdata[chOffs + 0x10] | (m_regdata[chOffs + 0x18] << 8); }
	[[nodiscard]] uint32_t ch_end(unsigned chOffs) const              { return m_regdata[chOffs + 0x20] | (m_regdata[chOffs + 0x28] << 8); }

private:
	std::array<uint8_t, REGISTERS> m_regdata;
};


class adpcm_a_channel
{
public:
	adpcm_a_channel(adpcm_a_registers& regs);

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void keyOnOff(bool on, unsigned chNum);
	void clock(unsigned chNum);

	// True when every sample this channel can produce is zero until the
	// next register write. clock() still has to run.
	[[nodiscard]] bool silent(unsigned chNum) const;

	// Stopped with a zero accumulator: clock() would only store that zero.
	[[nodiscard]] bool resting() const;

	// Register fields that sample() reads. A register write ends the current
	// buffer, so these hold for every sample of one generate().
	struct output_plan
	{
		int8_t mul;        // volume multiplier
		uint8_t shift;     // volume shift, accumulator downshift included
		uint8_t pan_mask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop. An empty pan mask means
	// this channel adds nothing.
	[[nodiscard]] output_plan make_output_plan(unsigned chNum) const;

	// Scaled sample for the current accumulator, which only clock() changes.
	[[nodiscard]] int16_t sample(const output_plan& plan) const;

private:
	adpcm_a_registers& m_regs; // reference to registers
	uint32_t m_curaddress = 0; // current address
	int16_t m_accumulator = 0; // 12-bit accumulator
	int8_t m_step_index = 0;   // index in the stepping table (0-48)
	bool m_playing = false;    // currently playing?
	uint8_t m_curnibble = 0;   // index of the current nibble
	uint8_t m_curbyte = 0;     // current byte of data
};


class adpcm_a_engine
{
public:
	static constexpr unsigned CHANNELS = adpcm_a_registers::CHANNELS;

	adpcm_a_engine();

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// Whole buffer, one channel at a time. envStart is the FM envelope counter
	// before this buffer, and this walks the same grid. A nullptr entry skips
	// output. A resting channel is not clocked.
	void generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t envStart);

	// True when this channel adds zero until the next register write.
	[[nodiscard]] bool silent(unsigned chNum) const { return m_channel[chNum].silent(chNum); }

	// write to the ADPCM-A registers
	void write(uint32_t regNum, uint8_t data);

	// set the start/end address for a channel (for hardcoded YM2608 percussion)
	void set_start_end(unsigned chNum, uint16_t start, uint16_t end);

	[[nodiscard]]       auto& regs()       { return m_regs; }
	[[nodiscard]] const auto& regs() const { return m_regs; }

private:
	adpcm_a_registers m_regs;
	std::array<adpcm_a_channel, CHANNELS> m_channel;
};


// ADPCM-B register map:
//
//      System-wide registers:
//           00 x------- Start of synthesis/analysis
//              -x------ Record
//              --x----- External/manual driving
//              ---x---- Repeat playback
//              ----x--- Speaker off
//              -------x Reset
//           01 x------- Pan left
//              -x------ Pan right
//              ----x--- Start conversion
//              -----x-- DAC enable
//              ------x- DRAM access (1=8-bit granularity; 0=1-bit)
//              -------x RAM/ROM (1=ROM, 0=RAM)
//           02 xxxxxxxx Start address (low)
//           03 xxxxxxxx Start address (high)
//           04 xxxxxxxx End address (low)
//           05 xxxxxxxx End address (high)
//           06 xxxxxxxx Prescale value (low)
//           07 -----xxx Prescale value (high)
//           08 xxxxxxxx CPU data/buffer
//           09 xxxxxxxx Delta-N frequency scale (low)
//           0a xxxxxxxx Delta-N frequency scale (high)
//           0b xxxxxxxx Level control
//           0c xxxxxxxx Limit address (low)
//           0d xxxxxxxx Limit address (high)
//           0e xxxxxxxx DAC data [YM2608/10]
//           0f xxxxxxxx PCM data [YM2608/10]
//           0e xxxxxxxx DAC data high [Y8950]
//           0f xx------ DAC data low [Y8950]
//           10 -----xxx DAC data exponent [Y8950]
//
class adpcm_b_registers
{
public:
	static constexpr uint32_t REGISTERS = 0x11;

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// direct read/write access
	[[nodiscard]] uint8_t read(uint32_t index) const { return m_regdata[index]; }
	void write(uint32_t index, uint8_t data) { m_regdata[index] = data; }

	// system-wide registers
	[[nodiscard]] bool execute() const     { return bitfield(m_regdata[0x00], 7) != 0; }
	[[nodiscard]] bool record() const      { return bitfield(m_regdata[0x00], 6) != 0; }
	[[nodiscard]] bool external() const    { return bitfield(m_regdata[0x00], 5) != 0; }
	[[nodiscard]] bool repeat() const      { return bitfield(m_regdata[0x00], 4) != 0; }
	[[nodiscard]] bool resetflag() const   { return bitfield(m_regdata[0x00], 0) != 0; }
	[[nodiscard]] bool pan_left() const    { return bitfield(m_regdata[0x01], 7) != 0; }
	[[nodiscard]] bool pan_right() const   { return bitfield(m_regdata[0x01], 6) != 0; }
	[[nodiscard]] bool dram_8bit() const   { return bitfield(m_regdata[0x01], 1) != 0; }
	[[nodiscard]] bool rom_ram() const     { return bitfield(m_regdata[0x01], 0) != 0; }
	[[nodiscard]] uint32_t start() const   { return m_regdata[0x02] | (m_regdata[0x03] << 8); }
	[[nodiscard]] uint32_t end() const     { return m_regdata[0x04] | (m_regdata[0x05] << 8); }
	[[nodiscard]] uint32_t cpudata() const { return m_regdata[0x08]; }
	[[nodiscard]] uint32_t delta_n() const { return m_regdata[0x09] | (m_regdata[0x0a] << 8); }
	[[nodiscard]] uint32_t level() const   { return m_regdata[0x0b]; }
	[[nodiscard]] uint32_t limit() const   { return m_regdata[0x0c] | (m_regdata[0x0d] << 8); }

private:
	std::array<uint8_t, REGISTERS> m_regdata;
};


class adpcm_b_channel
{
public:
	static constexpr int16_t STEP_MIN = 127;
	static constexpr int16_t STEP_MAX = 24576;
	static constexpr uint8_t STATUS_EOS = 0x01;
	static constexpr uint8_t STATUS_BRDY = 0x02;
	static constexpr uint8_t STATUS_PLAYING = 0x04;

	adpcm_b_channel(Ram& ram, adpcm_b_registers& regs);

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// signal key on/off
	void keyOnOff(bool on);

	// num clocks. Position steps that do not cross a nibble are applied in
	// one multiply. Each nibble is consume_nibble(), shared with generate().
	// A channel that is not decoding clears PLAYING once and returns.
	void clock_n(unsigned num);

	// True when every sample this channel can produce is zero until the
	// next register write. clock() still has to run.
	[[nodiscard]] bool silent() const;

	// Not playing, both interpolator ends already zero: clock() does not
	// change the accumulators or the playing flag.
	[[nodiscard]] bool resting() const;

	// Register fields that the scaling reads, plus the caller's shift. A
	// register write ends the current buffer, so these hold for every sample
	// of one generate().
	struct output_plan
	{
		uint32_t level;    // linear volume
		uint8_t pan_mask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop. An empty pan mask means
	// this channel adds nothing.
	[[nodiscard]] output_plan make_output_plan() const;

	// Interpolated and scaled sample for the current position, which every
	// clock advances. OPNA's extra output bit is folded into the 9-bit shift.
	[[nodiscard]] int32_t sample(const output_plan& plan) const;

	// num clocks into an interleaved stereo buffer. Reads the decode state
	// and the position step once, like clock_n() does.
	void generate(float* buffer, unsigned num, const output_plan& plan);

	// return the status register
	[[nodiscard]] uint8_t status() const { return m_status; }

	// handle special register reads
	[[nodiscard]] uint8_t read(uint32_t regNum);
	[[nodiscard]] uint8_t peek(uint32_t regNum) const;

	// handle special register writes
	void write(uint32_t regNum, uint8_t value);

private:
	// Register state that lets a clock advance the position.
	[[nodiscard]] bool decoding() const;

	// One clock with the buffer's position step, after decoding() held.
	// False when playback stopped at the end address.
	bool advance(uint32_t delta);

	// One bit per output this channel feeds, empty when it adds nothing.
	[[nodiscard]] uint8_t pan_mask() const;

	// helper - return the current address shift
	[[nodiscard]] uint32_t address_shift() const;

	// One nibble, after the fractional position has already wrapped.
	// Returns false when playback stops at the end address.
	[[nodiscard]] bool consume_nibble();

	// load the start address
	void load_start();

	// limit checker; stops at the last byte of the chunk described by address_shift()
	[[nodiscard]] bool at_limit() const;

	// end checker; stops at the last byte of the chunk described by address_shift()
	[[nodiscard]] bool at_end() const;

private:
	adpcm_b_registers& m_regs; // reference to registers
	Ram& ram;
	uint32_t m_curaddress = 0;       // current address
	uint16_t m_position = 0;         // current fractional position
	int16_t m_accumulator = 0;       // accumulator
	int16_t m_prev_accum = 0;        // previous accumulator (for linear interp)
	int16_t m_adpcm_step = STEP_MIN; // next forecast (STEP_MIN..STEP_MAX)
	uint8_t m_status = STATUS_BRDY;  // EOS / BRDY / PLAYING
	uint8_t m_curnibble = 0;         // index of the current nibble
	uint8_t m_curbyte = 0;           // current byte of data
	uint8_t m_dummy_read = 0;        // dummy read tracker
	bool m_cpu_write_active = false; // unfinished CPU RAM write sequence
};


class adpcm_b_engine
{
public:
	adpcm_b_engine(const DeviceConfig& config, std::string_view name);

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// Whole buffer for the single channel. A nullptr buffer skips output.
	// A resting channel returns without clocking.
	void generate(float* buffer, unsigned num);

	// True when this channel adds zero until the next register write.
	[[nodiscard]] bool silent() const { return m_channel.silent(); }

	// read from the ADPCM-B registers
	[[nodiscard]] uint8_t read(uint32_t regNum) { return m_channel.read(regNum); }
	[[nodiscard]] uint8_t peek(uint32_t regNum) const { return m_channel.peek(regNum); }

	// write to the ADPCM-B registers
	void write(uint32_t regNum, uint8_t data);

	// status
	[[nodiscard]] uint8_t status() const { return m_channel.status(); }

	[[nodiscard]]       auto& regs()       { return m_regs; }
	[[nodiscard]] const auto& regs() const { return m_regs; }

private:
	adpcm_b_registers m_regs;
	Ram ram;
	adpcm_b_channel m_channel;
};

class YM2608 final
{
public:
	YM2608(DeviceConfig& config, std::string_view name, EmuTime time);

	void reset(EmuTime time);
	uint8_t readPort(unsigned port, EmuTime time);
	uint8_t peekPort(unsigned port, EmuTime time) const;
	void writePort(unsigned port, uint8_t value, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t readStatus(EmuTime time);
	uint8_t readData(EmuTime time);
	uint8_t readStatusHi(EmuTime time);
	uint8_t statusHi() const;
	uint8_t readDataHi();

	void writeRegister(unsigned regNum, uint8_t data, EmuTime time);
	uint8_t peekRegister(unsigned regNum, EmuTime time) const;

	void updateStream(EmuTime time);
	void updatePrescale(uint8_t prescale);

	[[nodiscard]] unsigned prescale() const;
	[[nodiscard]] unsigned fmRate() const;
	[[nodiscard]] unsigned ssgRate() const;
	void applyRates(EmuTime time);

	void generateFM(std::span<float*> buffers, unsigned num);

	friend class fm_engine;

private:
	void setBusyEnd(EmuTime time, uint32_t clocks);
	[[nodiscard]] bool isBusy(EmuTime time) const;

	class FmPart final : public ResampledSoundDevice {
	public:
		FmPart(DeviceConfig& config, std::string_view name);
		~FmPart();

		void updateStream(EmuTime time); // expose SoundDevice::updateStream()
		void rate(unsigned value);

	private:
		void generateChannels(std::span<float*> buffers, unsigned num) override;
	};

	struct Registers final : SimpleDebuggable {
		Registers(MSXMotherBoard& board, std::string_view name);
		uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	};

	EmuTime busyEnd;

	uint16_t addressLatch = 0;
	uint8_t irqEnable = 0x1f;
	uint8_t flagControl = 0x1c;

	fm_engine fm;
	adpcm_a_engine adpcmA;
	adpcm_b_engine adpcmB;

	Registers registers;
	FmPart fmPart;
	AY8910 ssg; // ym2149
};

} // namespace openmsx

#endif
