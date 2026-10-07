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
namespace ym2608 {

[[nodiscard]] constexpr uint32_t bitfield(uint32_t value, int start, int length = 1)
{
	return (value >> start) & ((1 << length) - 1);
}

enum EnvelopeState : uint8_t
{
	EG_ATTACK,
	EG_DECAY,
	EG_SUSTAIN,
	EG_RELEASE,
	EG_STATES,
};

// keyon sources; actual keyon is an OR over all of these.
enum KeyOnType : uint8_t
{
	KEYON_NORMAL,
	KEYON_CSM,
};


// This class holds data that is computed once at the start of clocking
// and remains static during subsequent sound generation
struct OpDataCache
{
	// set phaseStep to this value to recalculate it each sample; needed
	// in the case of PM LFO changes
	static constexpr uint32_t PHASE_STEP_DYNAMIC = 1;

	uint32_t phaseStep;                    // phase step, or PHASE_STEP_DYNAMIC if PM is active
	uint16_t totalLevel;                   // total level * 8
	uint16_t blockFreq;                    // raw block frequency value (used to compute phaseStep)
	uint16_t egSustain;                    // sustain level, shifted up to envelope values
	int8_t detune;                          // detuning value (used to compute phaseStep)
	uint8_t multiple;                       // multiple value (x.1, used to compute phaseStep)
	std::array<uint8_t, EG_STATES> egRate; // envelope rate, including KSR
	uint8_t lfoPmSens;                    // LFO PM sensitivity (0-7)
	uint8_t ssgEgMode;                    // SSG-EG envelope shape (0-7)
	bool ssgEgEnable;                     // true if SSG-EG drives the envelope
	bool lfoAmEnable;                     // true if the operator follows the LFO AM offset
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
class OpnaRegisters
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
	[[nodiscard]] static constexpr unsigned channelOffset(unsigned chNum)
	{
		assert(chNum < CHANNELS);
		return (chNum % 3) + 0x100 * (chNum / 3);
	}

	// map operator number to register offset
	[[nodiscard]] static constexpr unsigned operatorOffset(unsigned opNum)
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
	[[nodiscard]] uint8_t read(uint16_t index) const { return regData[index]; }

	// handle writes to the register array
	bool write(uint16_t index, uint8_t data, unsigned& chan, unsigned& opMask);

	// A disabled LFO holds its counter at 0. Position 0 gives an AM value of
	// 0x3f, and music depends on that added attenuation: MegaDrive Venom plays
	// notes with the LFO globally disabled while enabling AM on the operators.
	void holdDisabledLfo();

	// Divider count for the current LFO rate; constant for a whole buffer.
	[[nodiscard]] uint32_t lfoMaxCount() const;

	// clock an enabled LFO, returning its PM value
	int32_t clockLfo(uint32_t maxCount);

	struct LfoState { uint32_t counter; uint8_t am; };
	[[nodiscard]] LfoState saveLfo() const;
	void restoreLfo(LfoState state);

	// AM shift for a channel's sensitivity; constant for a whole buffer
	[[nodiscard]] uint32_t lfoAmShift(unsigned chOffs) const;

	// return the AM offset from LFO for that shift
	[[nodiscard]] uint32_t lfoAmOffset(uint32_t amShift) const;

	// caching helpers
	void cacheOperatorData(unsigned chOffs, unsigned opOffs, OpDataCache& cache);

	// compute the phase step, given a PM value
	[[nodiscard]] static uint32_t computePhaseStep(const OpDataCache& cache, int32_t lfoRawPm);

	// system-wide registers
	[[nodiscard]] bool lfoEnable() const                       { return byte(0x22, 3, 1) != 0; }
	[[nodiscard]] uint32_t lfoRate() const                     { return byte(0x22, 0, 3); }
	[[nodiscard]] uint32_t timerAValue() const                { return word(0x24, 0, 8, 0x25, 0, 2); }
	[[nodiscard]] uint32_t timerBValue() const                { return byte(0x26, 0, 8); }
	[[nodiscard]] bool csm() const                              { return byte(0x27, 6, 2) == 2; }
	[[nodiscard]] bool multiFreq() const                       { return byte(0x27, 6, 2) != 0; }
	[[nodiscard]] bool resetTimerB() const                    { return byte(0x27, 5, 1) != 0; }
	[[nodiscard]] bool resetTimerA() const                    { return byte(0x27, 4, 1) != 0; }
	[[nodiscard]] bool enableTimerB() const                   { return byte(0x27, 3, 1) != 0; }
	[[nodiscard]] bool enableTimerA() const                   { return byte(0x27, 2, 1) != 0; }
	[[nodiscard]] bool loadTimerB() const                     { return byte(0x27, 1, 1) != 0; }
	[[nodiscard]] bool loadTimerA() const                     { return byte(0x27, 0, 1) != 0; }
	[[nodiscard]] uint32_t multiBlockFreq(unsigned num) const { return word(0xac, 0, 6, 0xa8, 0, 8, num); }

	// per-channel registers
	[[nodiscard]] uint32_t chBlockFreq(unsigned chOffs) const    { return word(0xa4, 0, 6, 0xa0, 0, 8, chOffs); }
	[[nodiscard]] uint32_t chFeedback(unsigned chOffs) const      { return byte(0xb0, 3, 3, chOffs); }
	[[nodiscard]] uint32_t chAlgorithm(unsigned chOffs) const     { return byte(0xb0, 0, 3, chOffs); }
	[[nodiscard]] bool chOutput0(unsigned chOffs) const          { return byte(0xb4, 7, 1, chOffs) != 0; }
	[[nodiscard]] bool chOutput1(unsigned chOffs) const          { return byte(0xb4, 6, 1, chOffs) != 0; }
	[[nodiscard]] uint32_t chLfoAmSens(unsigned chOffs) const   { return byte(0xb4, 4, 2, chOffs); }
	[[nodiscard]] uint32_t chLfoPmSens(unsigned chOffs) const   { return byte(0xb4, 0, 3, chOffs); }

	// per-operator registers
	[[nodiscard]] uint32_t opDetune(unsigned opOffs) const        { return byte(0x30, 4, 3, opOffs); }
	[[nodiscard]] uint32_t opMultiple(unsigned opOffs) const      { return byte(0x30, 0, 4, opOffs); }
	[[nodiscard]] uint32_t opTotalLevel(unsigned opOffs) const   { return byte(0x40, 0, 7, opOffs); }
	[[nodiscard]] uint32_t opKsr(unsigned opOffs) const           { return byte(0x50, 6, 2, opOffs); }
	[[nodiscard]] uint32_t opAttackRate(unsigned opOffs) const   { return byte(0x50, 0, 5, opOffs); }
	[[nodiscard]] uint32_t opDecayRate(unsigned opOffs) const    { return byte(0x60, 0, 5, opOffs); }
	[[nodiscard]] bool opLfoAmEnable(unsigned opOffs) const     { return byte(0x60, 7, 1, opOffs) != 0; }
	[[nodiscard]] uint32_t opSustainRate(unsigned opOffs) const  { return byte(0x70, 0, 5, opOffs); }
	[[nodiscard]] uint32_t opSustainLevel(unsigned opOffs) const { return byte(0x80, 4, 4, opOffs); }
	[[nodiscard]] uint32_t opReleaseRate(unsigned opOffs) const  { return byte(0x80, 0, 4, opOffs); }
	[[nodiscard]] bool opSsgEgEnable(unsigned opOffs) const     { return byte(0x90, 3, 1, opOffs) != 0; }
	[[nodiscard]] uint32_t opSsgEgMode(unsigned opOffs) const   { return byte(0x90, 0, 3, opOffs); }

private:
	// return a bitfield extracted from a byte
	[[nodiscard]] uint32_t byte(uint32_t offset, uint32_t start, uint32_t count, uint32_t extraOffset = 0) const
	{
		return bitfield(regData[offset + extraOffset], start, count);
	}

	// return a bitfield extracted from a pair of bytes, MSBs listed first
	[[nodiscard]] uint32_t word(uint32_t offset1, uint32_t start1, uint32_t count1, uint32_t offset2, uint32_t start2, uint32_t count2, uint32_t extraOffset = 0) const
	{
		return (byte(offset1, start1, count1, extraOffset) << count2) | byte(offset2, start2, count2, extraOffset);
	}

	uint32_t lfoCounter = 0;               // LFO counter
	uint8_t lfoAm = 0;                     // current LFO AM value
	std::array<uint8_t, REGISTERS> regData; // register data
};


// FmOperator represents an FM operator (or "slot" in FM parlance), which
// produces an output sine wave modulated by an envelope
class FmOperator
{
	// "quiet" value, used to optimize when we can skip doing work
	static constexpr uint32_t EG_QUIET = 0x380;

public:
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void reset();

	// prepare prior to clocking; the registers are read only here
	[[nodiscard]] bool prepare(OpnaRegisters& regs, unsigned chOffs, unsigned opOffs);

	// Release has reached maximum attenuation. A later key-on restarts phase.
	[[nodiscard]] bool finished() const;

	// Same condition prepare() reports, without refreshing the cache or the key.
	[[nodiscard]] bool audible() const;

	// master clocking function
	void clock(uint32_t envCounter, int32_t lfoRawPm);

	// return the current phase value
	[[nodiscard]] uint32_t phase() const { return phaseAcc >> 10; }

	// 14-bit signed volume, given phase modulation and an AM LFO offset
	[[nodiscard]] int32_t computeVolume(uint32_t phase, uint32_t amOffset) const;

	// key state control
	void keyOnOff(bool on, KeyOnType type);

private:
	void startAttack(bool isRestart = false);
	void startRelease();

	void clockKeystate(bool on);
	void clockSsgEgState();
	void clockEnvelope(uint32_t envCounter);
	void clockPhase(int32_t lfoRawPm);

	// return effective attenuation of the envelope
	[[nodiscard]] uint32_t envelopeAttenuation(uint32_t amOffset) const;

private:
	uint32_t phaseAcc = 0;                    // current phase value (10.10 format)
	uint16_t envAttenuation = 0x3ff;      // computed envelope attenuation (4.6 format)
	EnvelopeState envState = EG_RELEASE; // current envelope state
	bool ssgInverted = false;             // true if the output should be inverted
	bool keyState = false;                // current key state
	uint8_t keyOnLive = 0;                // live key on state (bit 0 = direct, bit 1 = CSM)
	OpDataCache cache{};                  // cached values for performance
};


// FmChannel represents an FM channel which combines the output of 2 or 4
// operators into a final result
class FmChannel
{
	// OPNA scales each carrier down by one bit before summing them
	static constexpr uint32_t OUTPUT_SHIFT = 1;

public:
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void reset();

	// signal key on/off to our operators
	void keyOnOff(uint32_t states, KeyOnType type);

	// prepare prior to clocking; the registers are read only here
	[[nodiscard]] bool prepare(OpnaRegisters& regs, unsigned chNum);

	// Every operator has finished its release.
	[[nodiscard]] bool finished() const;

	// Any operator would still produce sound.
	[[nodiscard]] bool audible() const;

	// Feedback shift for a skipped mix. After two clocks both slots hold the
	// last written sample, and further clocks leave them there.
	void quiesceFeedback(unsigned num);

	// master clocking function
	void clock(uint32_t envCounter, int32_t lfoRawPm);

	// Register fields that output4Op() reads. A register write ends the
	// current buffer, so these hold for every sample of one generate().
	struct OutputPlan
	{
		uint8_t op2in;        // opout[] index that modulates operator 2
		uint8_t op3in;        // opout[] index that modulates operator 3
		uint8_t op4in;        // opout[] index that modulates operator 4
		uint8_t carrierMask; // bit 0 = op1, bit 1 = op2, bit 2 = op3 in the sum
		uint8_t feedback;     // operator 1 self-feedback, 0 means none
		uint8_t outputMask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop.
	[[nodiscard]] OutputPlan makeOutputPlan(const OpnaRegisters& regs, unsigned chNum) const;

	// 4-operator output handler; the caller routes the result to the outputs
	// the plan enables
	int32_t output4Op(const OutputPlan& plan, uint32_t amOffset);

private:
	std::array<int16_t, 2> feedback{};   // feedback memory for operator 1
	int16_t feedbackIn = 0;             // next input value for op 1 feedback (set in output4Op)
	std::array<FmOperator, 4> ops;
};


// FmEngine represents a set of operators and channels which together
// form a Yamaha FM core; chips that implement other engines (ADPCM, wavetable,
// etc) take this output and combine it with the others externally
class FmEngine
{
public:
	static constexpr unsigned CHANNELS = OpnaRegisters::CHANNELS;
	static constexpr unsigned OPERATORS = OpnaRegisters::OPERATORS;
	static constexpr uint8_t STATUS_TIMER_A = 0x01;
	static constexpr uint8_t STATUS_TIMER_B = 0x02;

	FmEngine(MSXMotherBoard& motherboard, std::string_view name);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void reset(EmuTime time);

	// Envelope counter before the next generate(). ADPCM-A replays the same
	// step to decide which samples it clocks.
	[[nodiscard]] uint32_t envelopeCounter() const { return envCounter; }

	// Whole buffer, one channel at a time. prepare() runs when a register or
	// key changed; otherwise the current envelope state is enough. A channel
	// whose operators have all reached release attenuation 0x3ff is not
	// clocked. The envelope counter still advances for ADPCM-A. Callers pass
	// a real buffer per channel. Channels outside chanMask, and channels that
	// are quiet, are nulled.
	void generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t chanMask);

	void writeReg(uint16_t regNum, uint8_t data, EmuTime time);
	[[nodiscard]] uint8_t status() const;

	// set/reset bits in the status register, updating the IRQ status
	uint8_t setResetStatus(uint8_t set, uint8_t reset);

	// set the IRQ mask
	void setIrqMask(uint8_t mask) { irqMask = mask; checkInterrupts(); }

	// return the current clock prescale
	[[nodiscard]] uint32_t clockPrescale() const { return prescale; }

	// set prescale factor (2/3/6)
	void setClockPrescale(uint32_t value) { prescale = value; }

	[[nodiscard]] EmuTime getCurrentTime() const { return timers[0].getCurrentTime(); }

	[[nodiscard]] uint8_t readReg(uint16_t index) const { return registers.read(index); }

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

	void engineTimerExpired(unsigned tNum, EmuTime time);
	void checkInterrupts();
	void modeWrite(uint8_t data, EmuTime time);
	void updateTimer(unsigned which, bool enable, int32_t deltaClocks, EmuTime time);
	void scheduleTimer(unsigned timer, int32_t duration, EmuTime time);

private:
	IRQHelper irq;
	std::array<Timer, 2> timers;
	uint32_t envCounter = 0;              // envelope counter; low 2 bits are sub-counter
	uint8_t statusReg = 0;                 // current status register
	uint8_t prescale = 6;             // prescale factor (2/3/6)
	uint8_t irqMask = STATUS_TIMER_A | STATUS_TIMER_B;
	std::array<bool, 2> timerRunning{};   // current timer running state
	uint8_t totalClocks = 0;              // low 8 bits of the total number of clocks processed
	bool modified = false;                 // register or key changed since the last generate()
	OpnaRegisters registers;              // register accessor
	std::array<FmChannel, CHANNELS> channels;
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
class AdpcmARegisters
{
public:
	static constexpr unsigned CHANNELS = 6;
	static constexpr uint32_t REGISTERS = 0x30;

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	[[nodiscard]] uint8_t read(uint32_t index) const { return regData[index]; }
	void write(uint32_t index, uint8_t data) { regData[index] = data; }

	// system-wide registers
	[[nodiscard]] uint32_t totalLevel() const                        { return bitfield(regData[0x01], 0, 6); }

	// per-channel registers
	[[nodiscard]] bool chPanLeft(unsigned chOffs) const             { return bitfield(regData[chOffs + 0x08], 7) != 0; }
	[[nodiscard]] bool chPanRight(unsigned chOffs) const            { return bitfield(regData[chOffs + 0x08], 6) != 0; }
	[[nodiscard]] uint32_t chInstrumentLevel(unsigned chOffs) const { return bitfield(regData[chOffs + 0x08], 0, 5); }
	[[nodiscard]] uint32_t chStart(unsigned chOffs) const            { return regData[chOffs + 0x10] | (regData[chOffs + 0x18] << 8); }
	[[nodiscard]] uint32_t chEnd(unsigned chOffs) const              { return regData[chOffs + 0x20] | (regData[chOffs + 0x28] << 8); }

private:
	std::array<uint8_t, REGISTERS> regData;
};


class AdpcmAChannel
{
public:
	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	void keyOnOff(bool on, uint32_t start);
	void clock(uint32_t end);

	// True when every sample this channel can produce is zero until the
	// next register write. clock() still has to run.
	[[nodiscard]] bool silent(const AdpcmARegisters& regs, unsigned chNum) const;

	// Stopped with a zero accumulator: clock() would only store that zero.
	[[nodiscard]] bool resting() const;

	// Register fields that sample() reads. A register write ends the current
	// buffer, so these hold for every sample of one generate().
	struct OutputPlan
	{
		int8_t mul;        // volume multiplier
		uint8_t shift;     // volume shift, accumulator downshift included
		uint8_t panMask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop. An empty pan mask means
	// this channel adds nothing.
	[[nodiscard]] OutputPlan makeOutputPlan(const AdpcmARegisters& regs, unsigned chNum) const;

	// Scaled sample for the current accumulator, which only clock() changes.
	[[nodiscard]] int16_t sample(const OutputPlan& plan) const;

private:
	uint32_t curAddress = 0; // current address
	int16_t accumulator = 0; // 12-bit accumulator
	int8_t stepIndex = 0;   // index in the stepping table (0-48)
	bool playing = false;    // currently playing?
	uint8_t curNibble = 0;   // index of the current nibble
	uint8_t curByte = 0;     // current byte of data
};


class AdpcmAEngine
{
public:
	static constexpr unsigned CHANNELS = AdpcmARegisters::CHANNELS;

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// Whole buffer, one channel at a time. envStart is the FM envelope counter
	// before this buffer, and this walks the same grid. A nullptr entry skips
	// output. A resting channel is not clocked.
	void generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t envStart);

	// True when this channel adds zero until the next register write.
	[[nodiscard]] bool silent(unsigned chNum) const { return channels[chNum].silent(registers, chNum); }

	void writeReg(uint32_t regNum, uint8_t data);

	// set the start/end address for a channel (for hardcoded YM2608 percussion)
	void setStartEnd(unsigned chNum, uint16_t start, uint16_t end);

	[[nodiscard]] uint8_t readReg(uint32_t index) const { return registers.read(index); }

private:
	AdpcmARegisters registers;
	std::array<AdpcmAChannel, CHANNELS> channels;
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
class AdpcmBRegisters
{
public:
	static constexpr uint32_t REGISTERS = 0x11;

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// direct read/write access
	[[nodiscard]] uint8_t read(uint32_t index) const { return regData[index]; }
	void write(uint32_t index, uint8_t data) { regData[index] = data; }

	// system-wide registers
	[[nodiscard]] bool execute() const     { return bitfield(regData[0x00], 7) != 0; }
	[[nodiscard]] bool record() const      { return bitfield(regData[0x00], 6) != 0; }
	[[nodiscard]] bool external() const    { return bitfield(regData[0x00], 5) != 0; }
	[[nodiscard]] bool repeat() const      { return bitfield(regData[0x00], 4) != 0; }
	[[nodiscard]] bool resetFlag() const   { return bitfield(regData[0x00], 0) != 0; }
	[[nodiscard]] bool panLeft() const    { return bitfield(regData[0x01], 7) != 0; }
	[[nodiscard]] bool panRight() const   { return bitfield(regData[0x01], 6) != 0; }
	[[nodiscard]] bool dram8Bit() const   { return bitfield(regData[0x01], 1) != 0; }
	[[nodiscard]] bool romRam() const     { return bitfield(regData[0x01], 0) != 0; }
	[[nodiscard]] uint32_t start() const   { return regData[0x02] | (regData[0x03] << 8); }
	[[nodiscard]] uint32_t end() const     { return regData[0x04] | (regData[0x05] << 8); }
	[[nodiscard]] uint32_t cpuData() const { return regData[0x08]; }
	[[nodiscard]] uint32_t deltaN() const { return regData[0x09] | (regData[0x0a] << 8); }
	[[nodiscard]] uint32_t level() const   { return regData[0x0b]; }
	[[nodiscard]] uint32_t limit() const   { return regData[0x0c] | (regData[0x0d] << 8); }

private:
	std::array<uint8_t, REGISTERS> regData;
};


class AdpcmBChannel
{
public:
	static constexpr int16_t STEP_MIN = 127;
	static constexpr int16_t STEP_MAX = 24576;
	static constexpr uint8_t STATUS_EOS = 0x01;
	static constexpr uint8_t STATUS_BRDY = 0x02;
	static constexpr uint8_t STATUS_PLAYING = 0x04;

	AdpcmBChannel(Ram& ram, AdpcmBRegisters& regs);

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// signal key on/off
	void keyOnOff(bool on);

	// num clocks. Position steps that do not cross a nibble are applied in
	// one multiply. Each nibble is consumeNibble(), shared with generate().
	// A channel that is not decoding clears PLAYING once and returns.
	void clockN(unsigned num);

	// True when every sample this channel can produce is zero until the
	// next register write. clock() still has to run.
	[[nodiscard]] bool silent() const;

	// Not playing, both interpolator ends already zero: clock() does not
	// change the accumulators or the playing flag.
	[[nodiscard]] bool resting() const;

	// Register fields that the scaling reads, plus the caller's shift. A
	// register write ends the current buffer, so these hold for every sample
	// of one generate().
	struct OutputPlan
	{
		uint32_t level;    // linear volume
		uint8_t panMask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop. An empty pan mask means
	// this channel adds nothing.
	[[nodiscard]] OutputPlan makeOutputPlan() const;

	// Interpolated and scaled sample for the current position, which every
	// clock advances. OPNA's extra output bit is folded into the 9-bit shift.
	[[nodiscard]] int32_t sample(const OutputPlan& plan) const;

	// num clocks into an interleaved stereo buffer. Reads the decode state
	// and the position step once, like clockN() does.
	void generate(float* buffer, unsigned num, const OutputPlan& plan);

	// return the status register
	[[nodiscard]] uint8_t status() const { return statusReg; }

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
	[[nodiscard]] bool advance(uint32_t delta);

	// One bit per output this channel feeds, empty when it adds nothing.
	[[nodiscard]] uint8_t panMask() const;

	// helper - return the current address shift
	[[nodiscard]] uint32_t addressShift() const;

	// One nibble, after the fractional position has already wrapped.
	// Returns false when playback stops at the end address.
	[[nodiscard]] bool consumeNibble();

	// load the start address
	void loadStart();

	// limit checker; stops at the last byte of the chunk described by addressShift()
	[[nodiscard]] bool atLimit() const;

	// end checker; stops at the last byte of the chunk described by addressShift()
	[[nodiscard]] bool atEnd() const;

private:
	AdpcmBRegisters& registers; // reference to registers
	Ram& ram;
	uint32_t curAddress = 0;       // current address
	uint16_t position = 0;         // current fractional position
	int16_t accumulator = 0;       // accumulator
	int16_t prevAccum = 0;        // previous accumulator (for linear interp)
	int16_t adpcmStep = STEP_MIN; // next forecast (STEP_MIN..STEP_MAX)
	uint8_t statusReg = STATUS_BRDY;  // EOS / BRDY / PLAYING
	uint8_t curNibble = 0;         // index of the current nibble
	uint8_t curByte = 0;           // current byte of data
	uint8_t dummyRead = 0;        // dummy read tracker
	bool cpuWriteActive = false; // unfinished CPU RAM write sequence
};


class AdpcmBEngine
{
public:
	AdpcmBEngine(const DeviceConfig& config, std::string_view name);

	void reset();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// Whole buffer for the single channel. A nullptr buffer skips output.
	// A resting channel returns without clocking.
	void generate(float* buffer, unsigned num);

	// True when this channel adds zero until the next register write.
	[[nodiscard]] bool silent() const { return channel.silent(); }

	// read from the ADPCM-B data port (may advance address / update status)
	[[nodiscard]] uint8_t read(uint32_t regNum) { return channel.read(regNum); }
	[[nodiscard]] uint8_t peek(uint32_t regNum) const { return channel.peek(regNum); }

	void writeReg(uint32_t regNum, uint8_t data);

	[[nodiscard]] uint8_t status() const { return channel.status(); }
	[[nodiscard]] uint8_t readReg(uint32_t index) const { return registers.read(index); }

private:
	AdpcmBRegisters registers;
	Ram ram;
	AdpcmBChannel channel;
};

} // namespace ym2608

class YM2608
{
public:
	YM2608(DeviceConfig& config, std::string_view name, EmuTime time);

	void reset(EmuTime time);
	[[nodiscard]] uint8_t readPort(unsigned port, EmuTime time);
	[[nodiscard]] uint8_t peekPort(unsigned port, EmuTime time) const;
	void writePort(unsigned port, uint8_t value, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] uint8_t readStatus(EmuTime time);
	[[nodiscard]] uint8_t readData(EmuTime time);
	[[nodiscard]] uint8_t readStatusHi(EmuTime time);
	[[nodiscard]] uint8_t statusHi() const;
	[[nodiscard]] uint8_t readDataHi();

	void writeRegister(unsigned regNum, uint8_t data, EmuTime time);
	[[nodiscard]] uint8_t peekRegister(unsigned regNum, EmuTime time) const;

	void updateStream(EmuTime time);
	void updatePrescale(uint8_t prescale);
	void applyRates(EmuTime time);

	void generateFM(std::span<float*> buffers, unsigned num);

	friend class ym2608::FmEngine;

private:
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

	ym2608::FmEngine fm;
	ym2608::AdpcmAEngine adpcmA;
	ym2608::AdpcmBEngine adpcmB;

	Registers registers;
	FmPart fmPart;
	AY8910 ssg; // ym2149
};

} // namespace openmsx

#endif
