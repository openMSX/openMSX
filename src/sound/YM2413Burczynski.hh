#ifndef YM2413BURCZYNSKI_HH
#define YM2413BURCZYNSKI_HH

#include "YM2413Core.hh"

#include "FixedPoint.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>
#include <span>

namespace openmsx {
namespace YM2413Burczynski {

// sin wave entries
inline constexpr int SIN_BITS = 10;
inline constexpr size_t SIN_LEN  = 1 << SIN_BITS;
inline constexpr size_t SIN_MASK = SIN_LEN - 1;

class Channel;

/** 16.16 fixed point type for frequency calculations.
  */
using FreqIndex = FixedPoint<16>;

class Slot
{
public:
	Slot();

	void resetOperators();

	/** Update phase increment counter of operator.
	 * Also updates the EG rates if necessary.
	 */
	void updateGenerators(const Channel& channel);

	[[nodiscard]] int calcOutput(const Channel& channel, unsigned eg_cnt, bool carrier,
	                             unsigned lfo_am, int phase);
	[[nodiscard]] int calc_slot_mod(const Channel& channel, unsigned eg_cnt, bool carrier,
	                                unsigned lfo_pm, unsigned lfo_am);
	[[nodiscard]] int calc_envelope(const Channel& channel, unsigned eg_cnt, bool carrier);
	[[nodiscard]] int calc_phase(const Channel& channel, unsigned lfo_pm);

	enum class KeyPart : uint8_t { MAIN = 1, RHYTHM = 2 };
	void setKeyOn(KeyPart part);
	void setKeyOff(KeyPart part);
	void setKeyOnOff(KeyPart part, bool enabled);

	/** Does this slot currently produce an output signal?
	 */
	[[nodiscard]] bool isActive() const;

	/** Sets the frequency multiplier [0..15].
	 */
	void setFrequencyMultiplier(uint8_t value);

	/** Sets the key scale rate: true->0, false->2.
	 */
	void setKeyScaleRate(bool value);

	/** Sets the envelope type of the current instrument.
	  * @param value true->sustained, false->percussive.
	  */
	void setEnvelopeSustained(bool value);

	/** Enables (true) or disables (false) vibrato.
	 */
	void setVibrato(bool value);

	/** Enables (true) or disables (false) amplitude modulation.
	 */
	void setAmplitudeModulation(bool value);

	/** Sets the total level: [0..63].
	 */
	void setTotalLevel(const Channel& channel, uint8_t value);

	/** Sets the key scale level: 0->0 / 1->1.5 / 2->3.0 / 3->6.0 dB/OCT.
	 */
	void setKeyScaleLevel(const Channel& channel, uint8_t value);

	/** Sets the waveform: 0 = sinus, 1 = half sinus, half silence.
	 */
	void setWaveform(uint8_t value);

	/** Sets the amount of feedback [0..7].
	 */
	void setFeedbackShift(uint8_t value);

	/** Sets the attack rate [0..15].
	 */
	void setAttackRate(const Channel& channel, uint8_t value);

	/** Sets the decay rate [0..15].
	 */
	void setDecayRate(const Channel& channel, uint8_t value);

	/** Sets the release rate [0..15].
	 */
	void setReleaseRate(const Channel& channel, uint8_t value);

	/** Sets the sustain level [0..15].
	 */
	void setSustainLevel(uint8_t value);

	/** Called by Channel when block_fnum changes.
	 */
	void updateFrequency(const Channel& channel);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

public: // public for serialization, otherwise could be private
	enum class EnvelopeState : uint8_t {
		DUMP, ATTACK, DECAY, SUSTAIN, RELEASE, OFF
	};

private:
	/** Change envelope state
	 */
	void setEnvelopeState(EnvelopeState state);

	void updateTotalLevel(const Channel& channel);
	void updateAttackRate(int kcodeScaled);
	void updateDecayRate(int kcodeScaled);
	void updateReleaseRate(int kcodeScaled);

	std::span<const unsigned, SIN_LEN> waveTable;	// waveform select

	// Phase Generator
	FreqIndex phase{0}; // frequency counter
	FreqIndex freq{0};  // frequency counter step

	// Envelope Generator
	int TL{0};	// total level: TL << 2
	int TLL{0};	// adjusted now TL
	int egOut{0};	// envelope counter
	int sl{0};	// sustain level: sl_tab[SL]
	EnvelopeState state;

	std::array<int, 2> op1_out = {0, 0}; // MOD output for feedback
	bool eg_sustain{false};  // percussive/non-percussive mode
	uint8_t fb_shift{0}; // feedback shift value

	uint8_t key{0};	// 0 = KEY OFF, >0 = KEY ON

	std::span<const uint8_t, 8> eg_sel_dp;
	std::span<const uint8_t, 8> eg_sel_ar;
	std::span<const uint8_t, 8> eg_sel_dr;
	std::span<const uint8_t, 8> eg_sel_rr;
	std::span<const uint8_t, 8> eg_sel_rs;
	unsigned eg_mask_dp{0}; // == (1 << eg_sh_dp) - 1
	unsigned eg_mask_ar{0}; // == (1 << eg_sh_ar) - 1
	unsigned eg_mask_dr{0}; // == (1 << eg_sh_dr) - 1
	unsigned eg_mask_rr{0}; // == (1 << eg_sh_rr) - 1
	unsigned eg_mask_rs{0}; // == (1 << eg_sh_rs) - 1
	uint8_t eg_sh_dp{0};    // (dump state)
	uint8_t eg_sh_ar{0};    // (attack state)
	uint8_t eg_sh_dr{0};    // (decay state)
	uint8_t eg_sh_rr{0};    // (release state for non-perc.)
	uint8_t eg_sh_rs{0};    // (release state for perc.mode)

	uint8_t ar{0};	// attack rate: AR<<2
	uint8_t dr{0};	// decay rate:  DR<<2
	uint8_t rr{0};	// release rate:RR<<2
	uint8_t KSR{0};	// key scale rate
	uint8_t ksl{0};	// key scale level
	uint8_t mul{0};	// multiple: mul_tab[ML]

	// LFO
	uint8_t AMmask{0}; // LFO Amplitude Modulation enable mask
	uint8_t vib{0};    // LFO Phase Modulation enable flag (active high)
};

class Channel
{
public:
	/** Calculate the value of the current sample produced by this channel.
	 */
	[[nodiscard]] int calcOutput(unsigned eg_cnt, unsigned lfo_pm, unsigned lfo_am, int fm);

	/** Sets the frequency for this channel.
	 */
	void setFrequency(int block_fnum);

	/** Changes the lower 8 bits of the frequency for this channel.
	 */
	void setFrequencyLow(uint8_t value);

	/** Changes the higher 4 bits of the frequency for this channel.
	 */
	void setFrequencyHigh(uint8_t value);

	/** Sets some synthesis parameters as specified by the instrument.
	 * @param part Part [0..7] of the instrument.
	 * @param value New value for this part.
	 */
	void updateInstrumentPart(int part, uint8_t value);

	/** Sets all synthesis parameters as specified by the instrument.
	 * @param inst Instrument data.
	 */
	void updateInstrument(std::span<const uint8_t, 8> inst);

	[[nodiscard]] int getBlockFNum() const;
	[[nodiscard]] FreqIndex getFrequencyIncrement() const;
	[[nodiscard]] int getKeyScaleLevelBase() const;
	[[nodiscard]] uint8_t getKeyCode() const;
	[[nodiscard]] bool isSustained() const;
	void setSustain(bool sustained);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	Slot mod;
	Slot car;

private:
	// phase generator state
	int block_fnum{0}; // block+fnum
	FreqIndex fc{0};   // Freq. increment base
	int ksl_base{0};   // KeyScaleLevel Base step
	bool sus{false};   // sus on/off (release speed in percussive mode)
};

class YM2413 final : public YM2413Core
{
public:
	YM2413();

	// YM2413Core
	void reset() override;
	void writePort(bool port, uint8_t value, int offset) override;
	void pokeReg(uint8_t reg, uint8_t value) override;
	[[nodiscard]] std::span<const uint8_t, 64> peekRegs() const override;
	void generateChannels(std::span<float*, 9 + 5> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactor() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeReg(uint8_t reg, uint8_t value);

	/** Reset operator parameters.
	 */
	void resetOperators();

	[[nodiscard]] bool isRhythm() const;

	[[nodiscard]] Channel& getChannelForReg(uint8_t reg);

	/** Called when the custom instrument (instrument 0) has changed.
	 * @param part Part [0..7] of the instrument.
	 * @param value The new value.
	 */
	void updateCustomInstrument(int part, uint8_t value);

	void setRhythmFlags(uint8_t old);

private:
	/** OPLL chips have 9 channels. */
	std::array<Channel, 9> channels;

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	/** Random generator for noise: 23 bit shift register. */
	int noise_rng;

	/** Number of samples the output was completely silent. */
	unsigned idleSamples;

	using LFOAMIndex = FixedPoint< 6>;
	using LFOPMIndex = FixedPoint<10>;
	LFOAMIndex lfo_am_cnt{0};
	LFOPMIndex lfo_pm_cnt{0};

	/** Instrument settings:
	 *  0     - user instrument
	 *  1-15  - fixed instruments
	 *  16    - bass drum settings
	 *  17-18 - other percussion instruments
	 */
	std::array<std::array<uint8_t, 8>, 19> inst_tab;

	/** Registers */
	std::array<uint8_t, 0x40> reg;
	uint8_t registerLatch;
};

} // namespace YM2413Burczynski

SERIALIZE_CLASS_VERSION(YM2413Burczynski::YM2413, 4);
SERIALIZE_CLASS_VERSION(YM2413Burczynski::Channel, 3);
SERIALIZE_CLASS_VERSION(YM2413Burczynski::Slot, 2);

} // namespace openmsx

#endif
