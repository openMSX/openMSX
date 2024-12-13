#ifndef YMF278_HH
#define YMF278_HH

#include "ResampledSoundDevice.hh"

#include "EmuTime.hh"
#include "Rom.hh"
#include "SimpleDebuggable.hh"
#include "TrackedRam.hh"
#include "serialize_meta.hh"

#include "stl.hh"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace openmsx {

class DeviceConfig;

class YMF278 final : public ResampledSoundDevice
{
public:
	static constexpr auto k128 = size_t(128 * 1024); // 128kB
	using Block128 = std::span<const uint8_t, k128>; // a 128kB read-only memory block
	static constexpr auto nullBlock = std::span<const uint8_t, k128>(static_cast<const uint8_t*>(nullptr), k128);

	using SetupMemPtrFunc = std::function<void(
		bool /*mode0*/,
		std::span<const uint8_t> /*rom*/,
		std::span<const uint8_t> /*ram*/,
		std::span<YMF278::Block128, 32> /*memPtrs*/)>;

public:
	YMF278(const std::string& name, size_t ramSize, const DeviceConfig& config,
	       SetupMemPtrFunc setupMemPtrs_);
	~YMF278();
	void clearRam();
	void reset(EmuTime::param time);
	void writeReg(uint8_t reg, uint8_t data, EmuTime::param time);
	[[nodiscard]] uint8_t readReg(uint8_t reg);
	[[nodiscard]] uint8_t peekReg(uint8_t reg) const;

	[[nodiscard]] uint8_t readMem(unsigned address) const;
	void writeMem(unsigned address, uint8_t value);

	void setMixLevel(uint8_t x, EmuTime::param time);

	void setupMemoryPointers();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Slot {
	public:
		Slot();
		void reset();
		[[nodiscard]] uint8_t compute_rate(int val) const;
		[[nodiscard]] uint8_t compute_decay_rate(int val) const;
		[[nodiscard]] unsigned decay_rate(int num, int sample_rate);
		void envelope_next(int sample_rate);
		[[nodiscard]] int16_t compute_vib() const;
		[[nodiscard]] uint16_t compute_am() const;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		uint32_t startAddr;
		uint16_t loopAddr;
		uint16_t endAddr; // Note: stored in 2s complement (0x0000 = 0, 0x0001 = -65536, 0xffff = -1)
		uint32_t step;       // fixed-point frequency step
				     // invariant: step == calcStep(OCT, FN)
		uint32_t stepPtr;    // fixed-point pointer into the sample
		uint16_t pos;

		int16_t env_vol;

		uint32_t lfo_cnt;

		int16_t DL;
		uint16_t wave;		// wave table number
		uint16_t FN;		// f-number         TODO store 'FN | 1024'?
		int8_t OCT;		// octave [-8..+7]
		bool PRVB;		// pseudo-reverb
		uint8_t TLdest;		// destination total level
		uint8_t TL;		// total level  (goes towards TLdest)
		uint8_t pan;		// pan-pot 0..15
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
	void generateChannels(std::span<float*> bufs, unsigned num) override;

	void writeRegDirect(uint8_t reg, uint8_t data, EmuTime::param time);
	[[nodiscard]] unsigned getRamAddress(unsigned addr) const;
	[[nodiscard]] int16_t getSample(const Slot& slot, uint16_t pos) const;
	[[nodiscard]] static uint16_t nextPos(const Slot& slot, uint16_t pos, uint16_t increment);
	void advance();
	[[nodiscard]] bool anyActive();
	void keyOnHelper(Slot& slot) const;

	MSXMotherBoard& motherBoard;

	struct DebugRegisters final : SimpleDebuggable {
		DebugRegisters(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value, EmuTime::param time) override;
	} debugRegisters;

	struct DebugMemory final : SimpleDebuggable {
		DebugMemory(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value) override;
	} debugMemory;

	std::array<Slot, 24> slots;

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	int memAdr;

	Rom rom;
	TrackedRam ram;

	std::array<uint8_t, 256> regs;

	// To speed-up memory access we divide the 4MB address-space in 32 blocks of 128kB.
	// For each block we point to the corresponding region inside 'rom' or 'ram',
	// or 'nullBlock' means this region is unmapped.
	// Note: we can only _read_ via these pointers, not write.
	std::array<Block128, 32> memPtrs = generate_array<32>([](size_t) { return nullBlock; });

	// callback-function that fills in the above array
	SetupMemPtrFunc setupMemPtrs;
};
SERIALIZE_CLASS_VERSION(YMF278::Slot, 6);
SERIALIZE_CLASS_VERSION(YMF278, 4);

} // namespace openmsx

#endif
