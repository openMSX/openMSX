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

typedef FixedPoint<PM_FP_BITS> PhaseModulation;
typedef FixedPoint<EP_FP_BITS> EnvPhaseIndex;

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

	/** Sets the amount of feedback [0..7].
	 */
	void setFeedbackShift(byte value);

	/** Sets the Key Scale of Rate (0 or 1).
	 */
	void setKeyScaleRate(bool value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	bool AM, PM, EG;
	byte KR; // 0,1  transformed to  10,8
	byte ML; // 0-15
	byte KL; // 0-3
	byte TL; // 0-63
	byte FB; // 0,1-7  transformed to  0,7-1
	byte WF; // 0-1
	byte AR; // 0-15
	byte DR; // 0-15
	byte SL; // 0-15
	byte RR; // 0-15
};

class Slot {
public:
	void reset();

	inline void setEnvelopeState(EnvelopeState state);
	inline bool isActive() const;

	inline void slotOn();
	inline void slotOn2();
	inline void slotOff();
	inline void setPatch(Patch& patch);
	inline void setVolume(unsigned volume);

	template <bool HAS_PM> inline unsigned calc_phase(PhaseModulation lfo_pm);
	template <bool HAS_AM, bool FIXED_ENV>
	inline unsigned calc_envelope(int lfo_am, unsigned fixed_env);
	template <bool HAS_AM> unsigned calc_fixed_env() const;
	void calc_envelope_outline(unsigned& out);
	template <bool HAS_PM, bool HAS_AM, bool FIXED_ENV>
	inline int calc_slot_car(PhaseModulation lfo_pm, int lfo_am, int fm, unsigned fixed_env);
	template <bool HAS_PM, bool HAS_AM, bool HAS_FB, bool FIXED_ENV>
	inline int calc_slot_mod(PhaseModulation lfo_pm, int lfo_am, unsigned fixed_env);

	inline int calc_slot_tom();
	inline int calc_slot_snare(bool noise);
	inline int calc_slot_cym(unsigned phase7, unsigned phase8);
	inline int calc_slot_hat(unsigned phase7, unsigned phase8, bool noise);
	inline void updatePG(unsigned freq);
	inline void updateTLL(unsigned freq, bool actAsCarrier);
	inline void updateRKS(unsigned freq);
	inline void updateWF();
	inline void updateEG();
	inline void updateAll(unsigned freq, bool actAsCarrier);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	unsigned* sintbl;	// Wavetable (for PG)

	// OUTPUT
	int feedback;
	int output;		// Output value of slot

	// for Phase Generator (PG)
	unsigned cphase;	// Phase counter
	unsigned dphase;	// Phase increment

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
	inline void setVol(unsigned volume);
	inline void keyOn();
	inline void keyOff();

	Slot mod, car;
	unsigned patchFlags;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

class YM2413 : public YM2413Core
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
	virtual void reset();
	virtual void writeReg(byte reg, byte value);
	virtual byte peekReg(byte reg) const;
	virtual void generateChannels(int* bufs[9 + 5], unsigned num);
	virtual int getAmplificationFactor() const;

	/** Channel & Slot */
	Channel channels[9];

	/** Pitch Modulator */
	unsigned pm_phase;

	/** Amp Modulator */
	unsigned am_phase;

	/** Noise Generator */
	unsigned noise_seed;

	/** Number of samples the output was completely silent. */
	unsigned idleSamples;

	/** Voice Data */
	Patch patches[19][2];

	/** Registers */
	byte reg[0x40];
};

} // namespace YM2413Okazaki

SERIALIZE_CLASS_VERSION(YM2413Okazaki::Slot, 3);
SERIALIZE_CLASS_VERSION(YM2413Okazaki::Channel, 2);
SERIALIZE_CLASS_VERSION(YM2413Okazaki::YM2413, 2);

} // namespace openmsx

#endif
