#ifndef YM2413OKAZAKI_HH
#define YM2413OKAZAKI_HH

#include "YM2413Core.hh"
#include "FixedPoint.hh"
#include "serialize_meta.hh"

namespace openmsx {
namespace YM2413Okazaki {

class YM2413;

constexpr int EP_FP_BITS = 15;
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

	void initModulator(const uint8_t* data);
	void initCarrier  (const uint8_t* data);

	/** Sets the Key Scale of Rate (0 or 1). */
	inline void setKR(uint8_t value);
	/** Sets the frequency multiplier factor [0..15]. */
	inline void setML(uint8_t value);
	/** Sets Key scale level [0..3]. */
	inline void setKL(uint8_t value);
	/** Set volume (total level) [0..63]. */
	inline void setTL(uint8_t value);
	/** Set waveform [0..1]. */
	inline void setWF(uint8_t value);
	/** Sets the amount of feedback [0..7]. */
	inline void setFB(uint8_t value);
	/** Sets sustain level [0..15]. */
	inline void setSL(uint8_t value);

	const unsigned* WF; // 0-1    transformed to waveform[0-1]
	const uint8_t* KL;  // 0-3    transformed to tllTable[0-3]
	unsigned SL;        // 0-15   transformed to slTable[0-15]
	uint8_t AMPM;       // 0-3    2 packed booleans
	bool EG;            // 0-1
	uint8_t KR;         // 0-1    transformed to 10,8
	uint8_t ML;         // 0-15   transformed to mlTable[0-15]
	uint8_t TL;         // 0-63   transformed to TL2EG(0..63) == [0..252]
	uint8_t FB;         // 0,1-7  transformed to 0,7-1
	uint8_t AR;         // 0-15
	uint8_t DR;         // 0-15
	uint8_t RR;         // 0-15
};

class Slot {
public:
	void reset();

	inline void setEnvelopeState(EnvelopeState state);
	[[nodiscard]] inline bool isActive() const;

	inline void slotOn();
	inline void slotOn2();
	inline void slotOff();
	inline void setPatch(const Patch& patch);
	inline void setVolume(unsigned value);

	[[nodiscard]] inline unsigned calc_phase(unsigned lfo_pm);
	template<bool HAS_AM, bool FIXED_ENV>
	[[nodiscard]] inline unsigned calc_envelope(int lfo_am, unsigned fixed_env);
	template<bool HAS_AM> [[nodiscard]] unsigned calc_fixed_env() const;
	void calc_envelope_outline(unsigned& out);
	template<bool HAS_AM, bool FIXED_ENV>
	[[nodiscard]] inline int calc_slot_car(unsigned lfo_pm, int lfo_am, int fm, unsigned fixed_env);
	template<bool HAS_AM, bool HAS_FB, bool FIXED_ENV>
	[[nodiscard]] inline int calc_slot_mod(unsigned lfo_pm, int lfo_am, unsigned fixed_env);

	[[nodiscard]] inline int calc_slot_tom();
	[[nodiscard]] inline int calc_slot_snare(bool noise);
	[[nodiscard]] inline int calc_slot_cym(unsigned phase7, unsigned phase8);
	[[nodiscard]] inline int calc_slot_hat(unsigned phase7, unsigned phase8, bool noise);
	inline void updatePG(unsigned freq);
	inline void updateTLL(unsigned freq, bool actAsCarrier);
	inline void updateRKS(unsigned freq);
	inline void updateEG();
	inline void updateAll(unsigned freq, bool actAsCarrier);

	void serialize(Archive auto& ar, unsigned version);

	// OUTPUT
	int feedback;
	int output;		// Output value of slot

	// for Phase Generator (PG)
	unsigned cPhase;	// Phase counter
	unsigned dPhase[8];	// Phase increment

	// for Envelope Generator (EG)
	unsigned volume;             // Current volume
	unsigned tll;                // Total Level + Key scale level
	const int* dPhaseDRTableRks; // (converted to EnvPhaseIndex)
	EnvelopeState state;         // Current state
	EnvPhaseIndex eg_phase;      // Phase
	EnvPhaseIndex eg_dPhase;     // Phase increment amount
	EnvPhaseIndex eg_phase_max;
	uint8_t slot_on_flag;
	bool sustain;                // Sustain

	Patch patch;
	Slot* sibling; // pointer to sibling slot (only valid for car -> mod)
};

class Channel {
public:
	Channel();
	void reset(YM2413& ym2413);
	inline void setPatch(unsigned num, YM2413& ym2413);
	inline void setSustain(bool sustain, bool modActAsCarrier);
	inline void keyOn();
	inline void keyOff();

	Slot mod, car;

	void serialize(Archive auto& ar, unsigned version);
};

class YM2413 final : public YM2413Core
{
public:
	YM2413();

	// YM2413Core
	void reset() override;
	void writePort(bool port, uint8_t value, int offset) override;
	void pokeReg(uint8_t reg, uint8_t data) override;
	[[nodiscard]] uint8_t peekReg(uint8_t reg) const override;
	void generateChannels(float* bufs[9 + 5], unsigned num) override;
	[[nodiscard]] float getAmplificationFactor() const override;

	[[nodiscard]] Patch& getPatch(unsigned instrument, bool carrier);

	void serialize(Archive auto& ar, unsigned version);

private:
	void writeReg(uint8_t r, uint8_t data);

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
	inline void setRhythmFlags(uint8_t old);
	inline void update_key_status();
	[[nodiscard]] inline bool isRhythm() const;
	[[nodiscard]] inline unsigned getFreq(unsigned channel) const;

	template<unsigned FLAGS>
	inline void calcChannel(Channel& ch, float* buf, unsigned num);

private:
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
	uint8_t reg[0x40];
	uint8_t registerLatch;
};

} // namespace YM2413Okazaki

SERIALIZE_CLASS_VERSION(YM2413Okazaki::Slot, 4);
SERIALIZE_CLASS_VERSION(YM2413Okazaki::Channel, 2);
SERIALIZE_CLASS_VERSION(YM2413Okazaki::YM2413, 4);

} // namespace openmsx

#endif
