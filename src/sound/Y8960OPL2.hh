#ifndef Y8960OPL2_HH
#define Y8960OPL2_HH

#include "ResampledSoundDevice.hh"
#include "Y8960Adpcm.hh"
#include "Y8960Pan.hh"

#include "EmuTime.hh"
#include "EmuTimer.hh"
#include "FixedPoint.hh"
#include "IRQHelper.hh"
#include "SimpleDebuggable.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace openmsx {

class DeviceConfig;
class Y8960AdpcmMemory;

/** The OPL2 block of the Y8960: an OPL2 with the Y8950's ADPCM-B, a pan per
  * FM channel (F6h-FEh) and a pan for the ADPCM (FFh).
  *
  * The ADPCM is mixed into this block's output, so the mixer's OPL2 volume
  * covers it too.
  *
  * The rhythm sounds take the pan of the channel they are generated on: BD
  * channel 7, HH and SD channel 8, TOM and CYM channel 9, counting from 1.
  */
class Y8960OPL2 final : private ResampledSoundDevice, private EmuTimerCallback
{
public:
	static constexpr uint8_t PAN_BASE  = 0xF6; // F6h-FEh: FM channel 1-9
	static constexpr uint8_t PAN_ADPCM = 0xFF;

	static constexpr int CLOCK_FREQ     = 3579545;
	static constexpr int CLOCK_FREQ_DIV = 72;

	// Bitmask for register 0x04
	static constexpr int R04_ST1          = 0x01; // Timer1 Start
	static constexpr int R04_ST2          = 0x02; // Timer2 Start
	static constexpr int R04_MASK_BUF_RDY = 0x08; // Mask 'Buffer Ready'
	static constexpr int R04_MASK_EOS     = 0x10; // Mask 'End of sequence'
	static constexpr int R04_MASK_T2      = 0x20; // Mask Timer2 flag
	static constexpr int R04_MASK_T1      = 0x40; // Mask Timer1 flag
	static constexpr int R04_IRQ_RESET    = 0x80; // IRQ RESET

	// Bitmask for status register
	static constexpr int STATUS_PCM_BSY = 0x01;
	static constexpr int STATUS_EOS     = R04_MASK_EOS;
	static constexpr int STATUS_BUF_RDY = R04_MASK_BUF_RDY;
	static constexpr int STATUS_T2      = R04_MASK_T2;
	static constexpr int STATUS_T1      = R04_MASK_T1;

	Y8960OPL2(const std::string& name, const DeviceConfig& config,
	          Y8960AdpcmMemory& adpcmMemory, EmuTime time);
	~Y8960OPL2();

	void clearRam();
	void reset(EmuTime time);
	void writeReg(uint8_t rg, uint8_t data, EmuTime time);
	[[nodiscard]] uint8_t readReg(uint8_t rg, EmuTime time);
	[[nodiscard]] uint8_t peekReg(uint8_t rg, EmuTime time) const;
	[[nodiscard]] uint8_t readStatus(EmuTime time) const;
	[[nodiscard]] uint8_t peekStatus(EmuTime time) const;

	// called by the ADPCM block
	void setStatus(uint8_t flags);
	void resetStatus(uint8_t flags);
	[[nodiscard]] uint8_t peekRawStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;

	void keyOn_BD();
	void keyOn_SD();
	void keyOn_TOM();
	void keyOn_HH();
	void keyOn_CYM();
	void keyOff_BD();
	void keyOff_SD();
	void keyOff_TOM();
	void keyOff_HH();
	void keyOff_CYM();
	void setRythmMode(int data);
	void update_key_status();
	void updateWaveTables();
	void updatePanGains();

	[[nodiscard]] bool checkMuteHelper();

	void changeStatusMask(uint8_t newMask);

	void callback(uint8_t flag) override;

public:
	// Dynamic range of envelope
	static constexpr int EG_BITS = 9;

	// Bits for envelope phase incremental counter
	static constexpr int EG_DP_BITS = 23;
	using EnvPhaseIndex = FixedPoint<EG_DP_BITS - EG_BITS>;

	enum class EnvelopeState : uint8_t { ATTACK, DECAY, SUSTAIN, RELEASE, FINISH };

private:
	enum KeyPart : uint8_t { KEY_MAIN = 1, KEY_RHYTHM = 2 };

	class Patch {
	public:
		Patch();
		void reset();

		void setKeyScaleRate(bool value) {
			KR = value ? 9 : 11;
		}
		void setFeedbackShift(uint8_t value) {
			FB = value ? 8 - value : 0;
		}

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		bool AM, PM, EG;
		uint8_t KR; // 0,1   transformed to 9,11
		uint8_t ML; // 0-15
		uint8_t KL; // 0-3
		uint8_t TL; // 0-63
		uint8_t FB; // 0,1-7  transformed to 0,7-1
		uint8_t AR; // 0-15
		uint8_t DR; // 0-15
		uint8_t SL; // 0-15
		uint8_t RR; // 0-15
	};

	class Slot {
	public:
		Slot();
		void reset();

		[[nodiscard]] bool isActive() const;
		void slotOn (KeyPart part);
		void slotOff(KeyPart part);

		[[nodiscard]] unsigned calc_phase(int lfo_pm);
		[[nodiscard]] unsigned calc_envelope(int lfo_am);
		[[nodiscard]] int calc_slot_car(int lfo_pm, int lfo_am, int fm);
		[[nodiscard]] int calc_slot_mod(int lfo_pm, int lfo_am);
		[[nodiscard]] int calc_slot_tom(int lfo_pm, int lfo_am);
		[[nodiscard]] int calc_slot_snare(int lfo_pm, int lfo_am, int whiteNoise);
		[[nodiscard]] int calc_slot_cym(int lfo_am, int a, int b);
		[[nodiscard]] int calc_slot_hat(int lfo_am, int a, int b, int whiteNoise);

		void updateAll(unsigned freq);
		void updatePG(unsigned freq);
		void updateTLL(unsigned freq);
		void updateRKS(unsigned freq);
		void updateEG();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		// OUTPUT
		int feedback;
		int output;		// Output value of slot

		// for Phase Generator (PG)
		unsigned phase;		// Phase
		unsigned dPhase;	// Phase increment amount

		// for Envelope Generator (EG)
		std::span<const EnvPhaseIndex, 16> dPhaseARTableRks;
		std::span<const EnvPhaseIndex, 16> dPhaseDRTableRks;
		int tll;		// Total Level + Key scale level
		EnvelopeState eg_mode;  // Current state
		EnvPhaseIndex eg_phase;	// Phase
		EnvPhaseIndex eg_dPhase;// Phase increment amount

		Patch patch;
		uint8_t key;

		// OPL2 waveform select. 'wave' is what register 0xE0-0xF5 last
		// selected, 'waveTable' is the table actually in use: it only
		// follows 'wave' while bit 5 of the TEST register is set.
		uint8_t wave;
		const unsigned* waveTable;
	};

	class Channel {
	public:
		Channel();
		void reset();
		void setFreq(unsigned freq);
		void keyOn (KeyPart part);
		void keyOff(KeyPart part);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		std::array<Slot, 2> slot;
		unsigned freq; // combined F-Number and Block
		bool alg;
	};

	MSXMotherBoard& motherBoard;
	Y8960Adpcm adpcm;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;

	const std::unique_ptr<EmuTimer> timer1; //  80us timer
	const std::unique_ptr<EmuTimer> timer2; // 320us timer
	IRQHelper irq;

	std::array<uint8_t, 0x100> reg;

	std::array<Channel, 9> ch;

	// derived from reg[PAN_BASE..PAN_ADPCM]; the last one is the ADPCM's
	std::array<y8960::PanGain, 10> panGain;

	unsigned pm_phase; // Pitch Modulator
	unsigned am_phase; // Amp Modulator

	// Noise Generator
	int noise_seed;
	unsigned noiseA_phase;
	unsigned noiseB_phase;
	unsigned noiseA_dPhase;
	unsigned noiseB_dPhase;

	uint8_t status;     // STATUS Register
	uint8_t statusMask; // bit=0 -> masked
	bool rythm_mode;
	bool am_mode;
	bool pm_mode;
};

SERIALIZE_CLASS_VERSION(Y8960OPL2, 3);

} // namespace openmsx

#endif
