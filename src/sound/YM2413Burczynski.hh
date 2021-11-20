#ifndef YM2413BURCZYNSKI_HH
#define YM2413BURCZYNSKI_HH

#include "YM2413Core.hh"
#include "FixedPoint.hh"
#include "serialize_meta.hh"

namespace openmsx {
namespace YM2413Burczynski {

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
	void updateGenerators(Channel& channel);

	[[nodiscard]] inline int calcOutput(Channel& channel, unsigned eg_cnt, bool carrier,
	                      unsigned lfo_am, int phase);
	[[nodiscard]] inline int calc_slot_mod(Channel& channel, unsigned eg_cnt, bool carrier,
	                         unsigned lfo_pm, unsigned lfo_am);
	[[nodiscard]] inline int calc_envelope(Channel& channel, unsigned eg_cnt, bool carrier);
	[[nodiscard]] inline int calc_phase(Channel& channel, unsigned lfo_pm);

	enum KeyPart { KEY_MAIN = 1, KEY_RHYTHM = 2 };
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
	void setTotalLevel(Channel& channel, uint8_t value);

	/** Sets the key scale level: 0->0 / 1->1.5 / 2->3.0 / 3->6.0 dB/OCT.
	 */
	void setKeyScaleLevel(Channel& channel, uint8_t value);

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
	void updateFrequency(Channel& channel);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

public: // public for serialization, otherwise could be private
	/** Envelope Generator phases
	 * Note: These are ordered: phase constants are compared in the code.
	 */
	enum EnvelopeState {
		EG_DUMP, EG_ATTACK, EG_DECAY, EG_SUSTAIN, EG_RELEASE, EG_OFF
	};

private:
	/** Change envelope state
	 */
	void setEnvelopeState(EnvelopeState state);

	inline void updateTotalLevel(Channel& channel);
	inline void updateAttackRate(int kcodeScaled);
	inline void updateDecayRate(int kcodeScaled);
	inline void updateReleaseRate(int kcodeScaled);

	const unsigned* wavetable;	// waveform select

	// Phase Generator
	FreqIndex phase; // frequency counter
	FreqIndex freq;  // frequency counter step

	// Envelope Generator
	int TL;		// total level: TL << 2
	int TLL;	// adjusted now TL
	int egOut;	// envelope counter
	int sl;		// sustain level: sl_tab[SL]
	EnvelopeState state;

	int op1_out[2];   // MOD output for feedback
	bool eg_sustain;  // percussive/nonpercussive mode
	uint8_t fb_shift; // feedback shift value

	uint8_t key;	// 0 = KEY OFF, >0 = KEY ON

	const uint8_t* eg_sel_dp;
	const uint8_t* eg_sel_ar;
	const uint8_t* eg_sel_dr;
	const uint8_t* eg_sel_rr;
	const uint8_t* eg_sel_rs;
	unsigned eg_mask_dp; // == (1 << eg_sh_dp) - 1
	unsigned eg_mask_ar; // == (1 << eg_sh_ar) - 1
	unsigned eg_mask_dr; // == (1 << eg_sh_dr) - 1
	unsigned eg_mask_rr; // == (1 << eg_sh_rr) - 1
	unsigned eg_mask_rs; // == (1 << eg_sh_rs) - 1
	uint8_t eg_sh_dp;    // (dump state)
	uint8_t eg_sh_ar;    // (attack state)
	uint8_t eg_sh_dr;    // (decay state)
	uint8_t eg_sh_rr;    // (release state for non-perc.)
	uint8_t eg_sh_rs;    // (release state for perc.mode)

	uint8_t ar;	// attack rate: AR<<2
	uint8_t dr;	// decay rate:  DR<<2
	uint8_t rr;	// release rate:RR<<2
	uint8_t KSR;	// key scale rate
	uint8_t ksl;	// keyscale level
	uint8_t mul;	// multiple: mul_tab[ML]

	// LFO
	uint8_t AMmask;	// LFO Amplitude Modulation enable mask
	uint8_t vib;	// LFO Phase Modulation enable flag (active high)
};

class Channel
{
public:
	Channel();

	/** Calculate the value of the current sample produced by this channel.
	 */
	[[nodiscard]] inline int calcOutput(unsigned eg_cnt, unsigned lfo_pm, unsigned lfo_am, int fm);

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
	void updateInstrument(const uint8_t* inst);

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
	int block_fnum;	// block+fnum
	FreqIndex fc;	// Freq. increment base
	int ksl_base;	// KeyScaleLevel Base step
	bool sus;	// sus on/off (release speed in percussive mode)
};

class YM2413 final : public YM2413Core
{
public:
	YM2413();

	// YM2413Core
	void reset() override;
	void writePort(bool port, uint8_t value, int offset) override;
	void pokeReg(uint8_t reg, uint8_t value) override;
	[[nodiscard]] uint8_t peekReg(uint8_t reg) const override;
	void generateChannels(float* bufs[9 + 5], unsigned num) override;
	[[nodiscard]] float getAmplificationFactor() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeReg(uint8_t reg, uint8_t value);

	/** Reset operator parameters.
	 */
	void resetOperators();

	[[nodiscard]] inline bool isRhythm() const;

	[[nodiscard]] Channel& getChannelForReg(uint8_t reg);

	/** Called when the custom instrument (instrument 0) has changed.
	 * @param part Part [0..7] of the instrument.
	 * @param value The new value.
	 */
	void updateCustomInstrument(int part, uint8_t value);

	void setRhythmFlags(uint8_t old);

private:
	/** OPLL chips have 9 channels. */
	Channel channels[9];

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	/** Random generator for noise: 23 bit shift register. */
	int noise_rng;

	/** Number of samples the output was completely silent. */
	unsigned idleSamples;

	using LFOAMIndex = FixedPoint< 6>;
	using LFOPMIndex = FixedPoint<10>;
	LFOAMIndex lfo_am_cnt;
	LFOPMIndex lfo_pm_cnt;

	/** Instrument settings:
	 *  0     - user instrument
	 *  1-15  - fixed instruments
	 *  16    - bass drum settings
	 *  17-18 - other percussion instruments
	 */
	uint8_t inst_tab[19][8];

	/** Registers */
	uint8_t reg[0x40];
	uint8_t registerLatch;
};

} // namespace YM2413Burczynski

SERIALIZE_CLASS_VERSION(YM2413Burczynski::YM2413, 4);
SERIALIZE_CLASS_VERSION(YM2413Burczynski::Channel, 3);
SERIALIZE_CLASS_VERSION(YM2413Burczynski::Slot, 2);

} // namespace openmsx

#endif
