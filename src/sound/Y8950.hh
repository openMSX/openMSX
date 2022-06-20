#ifndef Y8950_HH
#define Y8950_HH

#include "Y8950Adpcm.hh"
#include "Y8950KeyboardConnector.hh"
#include "ResampledSoundDevice.hh"
#include "DACSound16S.hh"
#include "SimpleDebuggable.hh"
#include "IRQHelper.hh"
#include "EmuTimer.hh"
#include "EmuTime.hh"
#include "FixedPoint.hh"
#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXAudio;
class DeviceConfig;
class Y8950Periphery;

class Y8950 final : private ResampledSoundDevice, private EmuTimerCallback
{
public:
	static constexpr int CLOCK_FREQ     = 3579545;
	static constexpr int CLOCK_FREQ_DIV = 72;

	// Bitmask for register 0x04
	// Timer1 Start.
	static constexpr int R04_ST1          = 0x01;
	// Timer2 Start.
	static constexpr int R04_ST2          = 0x02;
	// not used
	//static constexpr int R04            = 0x04;
	// Mask 'Buffer Ready'.
	static constexpr int R04_MASK_BUF_RDY = 0x08;
	// Mask 'End of sequence'.
	static constexpr int R04_MASK_EOS     = 0x10;
	// Mask Timer2 flag.
	static constexpr int R04_MASK_T2      = 0x20;
	// Mask Timer1 flag.
	static constexpr int R04_MASK_T1      = 0x40;
	// IRQ RESET.
	static constexpr int R04_IRQ_RESET    = 0x80;

	// Bitmask for status register
	static constexpr int STATUS_PCM_BSY = 0x01;
	static constexpr int STATUS_EOS     = R04_MASK_EOS;
	static constexpr int STATUS_BUF_RDY = R04_MASK_BUF_RDY;
	static constexpr int STATUS_T2      = R04_MASK_T2;
	static constexpr int STATUS_T1      = R04_MASK_T1;

	Y8950(const std::string& name, const DeviceConfig& config,
	      unsigned sampleRam, EmuTime::param time, MSXAudio& audio);
	~Y8950();

	void setEnabled(bool enabled, EmuTime::param time);
	void clearRam();
	void reset(EmuTime::param time);
	void writeReg(byte rg, byte data, EmuTime::param time);
	[[nodiscard]] byte readReg(byte rg, EmuTime::param time);
	[[nodiscard]] byte peekReg(byte rg, EmuTime::param time) const;
	[[nodiscard]] byte readStatus(EmuTime::param time) const;
	[[nodiscard]] byte peekStatus(EmuTime::param time) const;

	// for ADPCM
	void setStatus(byte flags);
	void resetStatus(byte flags);
	[[nodiscard]] byte peekRawStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(float** bufs, unsigned num) override;

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
	inline void setRythmMode(int data);
	void update_key_status();

	[[nodiscard]] bool checkMuteHelper();

	void changeStatusMask(byte newMask);

	void callback(byte flag) override;

public:
	// Dynamic range of envelope
	static constexpr int EG_BITS = 9;

	// Bits for envelope phase incremental counter
	static constexpr int EG_DP_BITS = 23;
	using EnvPhaseIndex = FixedPoint<EG_DP_BITS - EG_BITS>;

	enum EnvelopeState { ATTACK, DECAY, SUSTAIN, RELEASE, FINISH };

private:
	enum KeyPart { KEY_MAIN = 1, KEY_RHYTHM = 2 };

	class Patch {
	public:
		Patch();
		void reset();

		void setKeyScaleRate(bool value) {
			KR = value ? 9 : 11;
		}
		void setFeedbackShift(byte value) {
			FB = value ? 8 - value : 0;
		}

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		bool AM, PM, EG;
		byte KR; // 0,1   transformed to 9,11
		byte ML; // 0-15
		byte KL; // 0-3
		byte TL; // 0-63
		byte FB; // 0,1-7  transformed to 0,7-1
		byte AR; // 0-15
		byte DR; // 0-15
		byte SL; // 0-15
		byte RR; // 0-15
	};

	class Slot {
	public:
		void reset();

		[[nodiscard]] inline bool isActive() const;
		inline void slotOn (KeyPart part);
		inline void slotOff(KeyPart part);

		[[nodiscard]] inline unsigned calc_phase(int lfo_pm);
		[[nodiscard]] inline unsigned calc_envelope(int lfo_am);
		[[nodiscard]] inline int calc_slot_car(int lfo_pm, int lfo_am, int fm);
		[[nodiscard]] inline int calc_slot_mod(int lfo_pm, int lfo_am);
		[[nodiscard]] inline int calc_slot_tom(int lfo_pm, int lfo_am);
		[[nodiscard]] inline int calc_slot_snare(int lfo_pm, int lfo_am, int whitenoise);
		[[nodiscard]] inline int calc_slot_cym(int lfo_am, int a, int b);
		[[nodiscard]] inline int calc_slot_hat(int lfo_am, int a, int b, int whitenoise);

		inline void updateAll(unsigned freq);
		inline void updatePG(unsigned freq);
		inline void updateTLL(unsigned freq);
		inline void updateRKS(unsigned freq);
		inline void updateEG();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		// OUTPUT
		int feedback;
		int output;		// Output value of slot

		// for Phase Generator (PG)
		unsigned phase;		// Phase
		unsigned dPhase;	// Phase increment amount

		// for Envelope Generator (EG)
		const EnvPhaseIndex* dPhaseARTableRks;
		const EnvPhaseIndex* dPhaseDRTableRks;
		int tll;		// Total Level + Key scale level
		EnvelopeState eg_mode;  // Current state
		EnvPhaseIndex eg_phase;	// Phase
		EnvPhaseIndex eg_dPhase;// Phase increment amount

		Patch patch;
		byte key;
	};

	class Channel {
	public:
		Channel();
		void reset();
		inline void setFreq(unsigned freq);
		inline void keyOn (KeyPart part);
		inline void keyOff(KeyPart part);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		Slot slot[2];
		unsigned freq; // combined F-Number and Block
		bool alg;
	};

	MSXMotherBoard& motherBoard;
	Y8950Periphery& periphery;
	Y8950Adpcm adpcm;
	Y8950KeyboardConnector connector;
	DACSound16S dac13; // 13-bit (exponential) DAC

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	const std::unique_ptr<EmuTimer> timer1; //  80us timer
	const std::unique_ptr<EmuTimer> timer2; // 320us timer
	IRQHelper irq;

	byte reg[0x100];

	Channel ch[9];

	unsigned pm_phase; // Pitch Modulator
	unsigned am_phase; // Amp Modulator

	// Noise Generator
	int noise_seed;
	unsigned noiseA_phase;
	unsigned noiseB_phase;
	unsigned noiseA_dPhase;
	unsigned noiseB_dPhase;

	byte status;     // STATUS Register
	byte statusMask; // bit=0 -> masked
	bool rythm_mode;
	bool am_mode;
	bool pm_mode;
	bool enabled;
};

} // namespace openmsx

#endif
