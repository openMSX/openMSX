#ifndef YMF278_HH
#define YMF278_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"
#include "Rom.hh"
#include "TrackedRam.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <string>

namespace openmsx {

class DeviceConfig;

class YMF278 final : public ResampledSoundDevice
{
public:
	YMF278(const std::string& name, int ramSize,
	       const DeviceConfig& config);
	~YMF278();
	void clearRam();
	void reset(EmuTime::param time);
	void writeReg(byte reg, byte data, EmuTime::param time);
	[[nodiscard]] byte readReg(byte reg);
	[[nodiscard]] byte peekReg(byte reg) const;

	[[nodiscard]] byte readMem(unsigned address) const;
	void writeMem(unsigned address, byte value);

	void setMixLevel(uint8_t x, EmuTime::param time);

	void serialize(Archive auto& ar, unsigned version);

private:
	class Slot {
	public:
		Slot();
		void reset();
		[[nodiscard]] int compute_rate(int val) const;
		[[nodiscard]] int compute_decay_rate(int val) const;
		[[nodiscard]] unsigned decay_rate(int num, int sample_rate);
		void envelope_next(int sample_rate);
		[[nodiscard]] int16_t compute_vib() const;
		[[nodiscard]] uint16_t compute_am() const;

		void serialize(Archive auto& ar, unsigned version);

		uint32_t startaddr;
		uint16_t loopaddr;
		uint16_t endaddr; // Note: stored in 2s complement (0x0000 = 0, 0x0001 = -65536, 0xffff = -1)
		uint32_t step;       // fixed-point frequency step
				     // invariant: step == calcStep(OCT, FN)
		uint32_t stepptr;    // fixed-point pointer into the sample
		uint16_t pos;

		int16_t env_vol;

		uint32_t lfo_cnt;

		int16_t DL;
		uint16_t wave;		// wavetable number
		uint16_t FN;		// f-number         TODO store 'FN | 1024'?
		int8_t OCT;		// octave [-8..+7]
		bool PRVB;		// pseudo-reverb
		uint8_t TLdest;		// destination total level
		uint8_t TL;		// total level  (goes towards TLdest)
		uint8_t pan;		// panpot 0..15
		bool keyon;		// slot keyed on
		bool DAMP;
		uint8_t lfo;            // LFO speed 0..7
		uint8_t vib;		// vibrato 0..7
		uint8_t AM;		// AM level 0..7
		uint8_t AR;		// 0..15
		uint8_t D1R;		// 0..15
		uint8_t D2R;		// 0..15
		uint8_t RC;		// rate correction 0..15
		uint8_t RR;		// 0..15

		uint8_t bits;		// width of the samples

		uint8_t state;		// envelope generator state
		bool lfo_active;
	};

	// SoundDevice
	void generateChannels(float** bufs, unsigned num) override;

	void writeRegDirect(byte reg, byte data, EmuTime::param time);
	[[nodiscard]] unsigned getRamAddress(unsigned addr) const;
	[[nodiscard]] int16_t getSample(Slot& slot, uint16_t pos) const;
	[[nodiscard]] static uint16_t nextPos(Slot& slot, uint16_t pos, uint16_t increment);
	void advance();
	[[nodiscard]] bool anyActive();
	void keyOnHelper(Slot& slot);

	MSXMotherBoard& motherBoard;

	struct DebugRegisters final : SimpleDebuggable {
		DebugRegisters(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debugRegisters;

	struct DebugMemory final : SimpleDebuggable {
		DebugMemory(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debugMemory;

	Slot slots[24];

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	int memAdr;

	Rom rom;
	TrackedRam ram;

	byte regs[256];
};
SERIALIZE_CLASS_VERSION(YMF278::Slot, 6);
SERIALIZE_CLASS_VERSION(YMF278, 4);

} // namespace openmsx

#endif
