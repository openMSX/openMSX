// $Id$

#ifndef YM2413BURCZYNSKI_HH
#define YM2413BURCZYNSKI_HH

#include "YM2413Core.hh"
#include "FixedPoint.hh"
#include "serialize_meta.hh"

namespace openmsx {
namespace YM2413Burczynski {

class YM2413;
class Channel;

/** 16.16 fixed point type for frequency calculations.
  */
typedef FixedPoint<16> FreqIndex;

class Slot
{
public:
	Slot();

	void resetOperators();

	/** Update phase increment counter of operator.
	 * Also updates the EG rates if necessary.
	 */
	void updateGenerators(Channel& channel);

	inline int calcOutput(Channel& channel, unsigned eg_cnt, bool carrier,
	                      unsigned lfo_am, int phase);
	inline int calc_slot_mod(Channel& channel, unsigned eg_cnt, bool carrier,
	                         unsigned lfo_pm, unsigned lfo_am);
	inline int calc_envelope(Channel& channel, unsigned eg_cnt, bool carrier);
	inline int calc_phase(Channel& channel, unsigned lfo_pm);

	enum KeyPart { KEY_MAIN = 1, KEY_RHYTHM = 2 };
	void setKeyOn(KeyPart part);
	void setKeyOff(KeyPart part);
	void setKeyOnOff(KeyPart part, bool enabled);

	/** Does this slot currently produce an output signal?
	 */
	bool isActive() const;

	/** Sets the frequency multiplier [0..15].
	 */
	void setFrequencyMultiplier(byte value);

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
	void setTotalLevel(Channel& channel, byte value);

	/** Sets the key scale level: 0->0 / 1->1.5 / 2->3.0 / 3->6.0 dB/OCT.
	 */
	void setKeyScaleLevel(Channel& channel, byte value);

	/** Sets the waveform: 0 = sinus, 1 = half sinus, half silence.
	 */
	void setWaveform(byte value);

	/** Sets the amount of feedback [0..7].
	 */
	void setFeedbackShift(byte value);

	/** Sets the attack rate [0..15].
	 */
	void setAttackRate(byte value);

	/** Sets the decay rate [0..15].
	 */
	void setDecayRate(byte value);

	/** Sets the release rate [0..15].
	 */
	void setReleaseRate(byte value);

	/** Sets the sustain level [0..15].
	 */
	void setSustainLevel(byte value);

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
	inline void updateAttackRate();
	inline void updateDecayRate();
	inline void updateReleaseRate();

	unsigned* wavetable;	// waveform select

	// Phase Generator
	FreqIndex phase;	// frequency counter
	FreqIndex freq;	// frequency counter step

	// Envelope Generator
	int TL;		// total level: TL << 2
	int TLL;	// adjusted now TL
	int egout;	// envelope counter
	int sl;		// sustain level: sl_tab[SL]
	EnvelopeState state;

	int op1_out[2];	// MOD output for feedback
	bool eg_sustain;// percussive/nonpercussive mode
	byte fb_shift;	// feedback shift value

	byte key;	// 0 = KEY OFF, >0 = KEY ON

	byte eg_sh_dp;	// (dump state)
	byte eg_sel_dp;	// (dump state)
	byte eg_sh_ar;	// (attack state)
	byte eg_sel_ar;	// (attack state)
	byte eg_sh_dr;	// (decay state)
	byte eg_sel_dr;	// (decay state)
	byte eg_sh_rr;	// (release state for non-perc.)
	byte eg_sel_rr;	// (release state for non-perc.)
	byte eg_sh_rs;	// (release state for perc.mode)
	byte eg_sel_rs;	// (release state for perc.mode)

	byte ar;	// attack rate: AR<<2
	byte dr;	// decay rate:  DR<<2
	byte rr;	// release rate:RR<<2
	byte KSR;	// key scale rate
	byte ksl;	// keyscale level
	byte kcodeScaled;	// key scale rate: kcode>>KSR
	byte mul;	// multiple: mul_tab[ML]

	// LFO
	byte AMmask;	// LFO Amplitude Modulation enable mask
	byte vib;	// LFO Phase Modulation enable flag (active high)
};

class Channel
{
public:
	Channel();

	/** Calculate the value of the current sample produced by this channel.
	 */
	inline int calcOutput(unsigned eg_cnt, unsigned lfo_pm, unsigned lfo_am, int fm);

	/** Sets the frequency for this channel.
	 */
	void setFrequency(int block_fnum);

	/** Changes the lower 8 bits of the frequency for this channel.
	 */
	void setFrequencyLow(byte value);

	/** Changes the higher 4 bits of the frequency for this channel.
	 */
	void setFrequencyHigh(byte value);

	/** Sets some synthesis parameters as specified by the instrument.
	 * @param part Part [0..7] of the instrument.
	 * @param value New value for this part.
	 */
	void updateInstrumentPart(int part, byte value);

	/** Sets all synthesis parameters as specified by the instrument.
	 * @param global the actual YM2413 core, private implementation
	 * @param instrument Number of the instrument.
	 */
	void updateInstrument(YM2413& global, int instrument);

	/** Sets all synthesis parameters as specified by the current instrument.
	 * The current instrument is determined by instvol_r.
	 * @param global the actual YM2413 core, private implementation
	 */
	void updateInstrument(YM2413& global);

	int getBlockFNum() const;
	FreqIndex getFrequencyIncrement() const;
	int getKeyScaleLevelBase() const;
	byte getKeyCode() const;
	bool isSustained() const;
	void setSustain(bool sustained);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	Slot mod;
	Slot car;

	/** Instrument/volume (or volume/volume in rhythm mode). */
	byte instvol_r;

private:
	// phase generator state
	int block_fnum;	// block+fnum
	FreqIndex fc;	// Freq. freqement base
	int ksl_base;	// KeyScaleLevel Base step
	byte kcode;	// key code (for key scaling)
	bool sus;	// sus on/off (release speed in percussive mode)
};

class YM2413 : public YM2413Core
{
public:
	YM2413();

	const byte* getInstrument(int instrument) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// YM2413Core
	virtual void reset();
	virtual void writeReg(byte reg, byte value);
	virtual byte peekReg(byte reg) const;
	virtual void generateChannels(int* bufs[9 + 5], unsigned num);
	virtual int getAmplificationFactor() const;

	/** Reset operator parameters.
	 */
	void resetOperators();

	int getNumMelodicChannels() const;

	Channel& getChannelForReg(byte reg);

	/** Called when the custom instrument (instrument 0) has changed.
	 * @param part Part [0..7] of the instrument.
	 * @param value The new value.
	 */
	void updateCustomInstrument(int part, byte value);

	void setRhythmMode(bool newMode);
	void setRhythmFlags(byte flags);

	/** OPLL chips have 9 channels. */
	Channel channels[9];

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	/** Random generator for noise: 23 bit shift register. */
	int noise_rng;

	/** Number of samples the output was completely silent. */
	unsigned idleSamples;

	typedef FixedPoint< 6> LFOAMIndex;
	typedef FixedPoint<10> LFOPMIndex;
	LFOAMIndex lfo_am_cnt;
	LFOPMIndex lfo_pm_cnt;

	/** Instrument settings:
	 *  0     - user instrument
	 *  1-15  - fixed instruments
	 *  16    - bass drum settings
	 *  17-18 - other percussion instruments
	 */
	byte inst_tab[19][8];

	/** Registers */
	byte reg[0x40];

	/** Rhythm mode. */
	bool rhythm;
};

} // namespace YM2413Burczynski

SERIALIZE_CLASS_VERSION(YM2413Burczynski::YM2413, 2);

} // namespace openmsx

#endif
