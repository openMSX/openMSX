#ifndef YM2413OKAZAKI_HH
#define YM2413OKAZAKI_HH

#include "YM2413Core.hh"
#include "FixedPoint.hh"
#include "serialize_meta.hh"

namespace openmsx {
namespace YM2413Okazaki {

// Constants (shared between this class and table generator)
#include "YM2413OkazakiConfig.hh"

class YM2413;

using EnvPhaseIndex = FixedPoint<EP_FP_BITS>;

enum EnvelopeState {
	ATTACK, DECAY, SUSHOLD, SUSTAIN, RELEASE, SETTLE, FINISH
};

class Patch {
public:
	/** Creates an uninitialized Patch object; call initXXX() before use.
	  * This approach makes it possible to create an array of Patches.
	  */
	Patch();

	void initModulator(const byte* data);
	void initCarrier  (const byte* data);

	/** Sets the Key Scale of Rate (0 or 1). */
	inline void setKR(byte value);
	/** Sets the frequency multiplier factor [0..15]. */
	inline void setML(byte value);
	/** Sets Key scale level [0..3]. */
	inline void setKL(byte value);
	/** Set volume (total level) [0..63]. */
	inline void setTL(byte value);
	/** Set waveform [0..1]. */
	inline void setWF(byte value);
	/** Sets the amount of feedback [0..7]. */
	inline void setFB(byte value);
	/** Sets sustain level [0..15]. */
	inline void setSL(byte value);

	unsigned* WF; // 0-1    transformed to waveform[0-1]
	byte* KL;     // 0-3    transformed to tllTable[0-3]
	unsigned SL;  // 0-15   transformed to slTable[0-15]
	byte AMPM;    // 0-3    2 packed booleans
	bool EG;      // 0-1
	byte KR;      // 0-1    transformed to 10,8
	byte ML;      // 0-15   transformed to mlTable[0-15]
	byte TL;      // 0-63   transformed to TL2EG(0..63) == [0..252]
	byte FB;      // 0,1-7  transformed to 0,7-1
	byte AR;      // 0-15
	byte DR;      // 0-15
	byte RR;      // 0-15
};

class Slot {
public:
	void reset();

	inline void setEnvelopeState(EnvelopeState state);
	inline bool isActive() const;

	inline void slotOn();
	inline void slotOn2();
	inline void slotOff();
	inline void setPatch(const Patch& patch);
	inline void setVolume(unsigned volume);

	inline unsigned calc_phase(unsigned lfo_pm);
	template <bool HAS_AM, bool FIXED_ENV>
	inline unsigned calc_envelope(int lfo_am, unsigned fixed_env);
	template <bool HAS_AM> unsigned calc_fixed_env() const;
	void calc_envelope_outline(unsigned& out);
	template<bool HAS_AM, bool FIXED_ENV>
	inline int calc_slot_car(unsigned lfo_pm, int lfo_am, int fm, unsigned fixed_env);
	template<bool HAS_AM, bool HAS_FB, bool FIXED_ENV>
	inline int calc_slot_mod(unsigned lfo_pm, int lfo_am, unsigned fixed_env);

	inline int calc_slot_tom();
	inline int calc_slot_snare(bool noise);
	inline int calc_slot_cym(unsigned phase7, unsigned phase8);
	inline int calc_slot_hat(unsigned phase7, unsigned phase8, bool noise);
	inline void updatePG(unsigned freq);
	inline void updateTLL(unsigned freq, bool actAsCarrier);
	inline void updateRKS(unsigned freq);
	inline void updateEG();
	inline void updateAll(unsigned freq, bool actAsCarrier);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// OUTPUT
	int feedback;
	int output;		// Output value of slot

	// for Phase Generator (PG)
	unsigned cphase;	// Phase counter
	unsigned dphase[8];	// Phase increment

	// for Envelope Generator (EG)
	unsigned volume;	// Current volume
	unsigned tll;		// Total Level + Key scale level
	int* dphaseDRTableRks;  // (converted to EnvPhaseIndex)
	EnvelopeState state;	// Current state
	EnvPhaseIndex eg_phase;	// Phase
	EnvPhaseIndex eg_dphase;// Phase increment amount
	EnvPhaseIndex eg_phase_max;
	byte slot_on_flag;
	bool sustain;		// Sustain

	Patch patch;
	Slot* sibling; // pointer to sibling slot (only valid for car -> mod)
};

class Channel {
public:
	Channel();
	void reset(YM2413& global);
	inline void setPatch(unsigned num, YM2413& global);
	inline void setSustain(bool sustain, bool modActAsCarrier);
	inline void keyOn();
	inline void keyOff();

	Slot mod, car;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

class YM2413 final : public YM2413Core
{
public:
	YM2413();

	inline void keyOn_BD();
	inline void keyOn_SD();
	inline void keyOn_TOM();
	inline void keyOn_HH();
	inline void keyOn_CYM();
	inline void keyOff_BD();
	inline void keyOff_SD();
	inline void keyOff_TOM();
	inline void keyOff_HH();
	inline void keyOff_CYM();
	inline void setRhythmFlags(byte old);
	inline void update_key_status();
	inline bool isRhythm() const;
	inline unsigned getFreq(unsigned channel) const;
	Patch& getPatch(unsigned instrument, bool carrier);

	template <unsigned FLAGS>
	inline void calcChannel(Channel& ch, int* buf, unsigned num);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// YM2413Core
	void reset() override;
	void writeReg(byte reg, byte value) override;
	byte peekReg(byte reg) const override;
	void generateChannels(int* bufs[9 + 5], unsigned num) override;
	int getAmplificationFactor() const override;

	/** Channel & Slot */
	Channel channels[9];

	/** Pitch Modulator */
	unsigned pm_phase;

	/** Amp Modulator */
	unsigned am_phase;

	/** Noise Generator */
	unsigned noise_seed;

	/** Voice Data */
	Patch patches[19][2];

	/** Registers */
	byte reg[0x40];
};

} // namespace YM2413Okazaki

SERIALIZE_CLASS_VERSION(YM2413Okazaki::Slot, 4);
SERIALIZE_CLASS_VERSION(YM2413Okazaki::Channel, 2);
SERIALIZE_CLASS_VERSION(YM2413Okazaki::YM2413, 3);

} // namespace openmsx

#endif
