#ifndef SCC_HH
#define SCC_HH

#include "ResampledSoundDevice.hh"

#include "SimpleDebuggable.hh"

#include <array>
#include <cstdint>
#include <vector>

namespace openmsx {

class SCC final : public ResampledSoundDevice
{
public:
	enum class Mode : uint8_t {Real, Compatible, Plus};

	SCC(const std::string& name, const DeviceConfig& config,
	    EmuTime time, Mode mode = Mode::Real);
	~SCC();

	// interaction with realCartridge
	void powerUp(EmuTime time);
	void reset(EmuTime time);
	[[nodiscard]] uint8_t readMem(uint8_t address,EmuTime time);
	[[nodiscard]] uint8_t peekMem(uint8_t address,EmuTime time) const;
	void writeMem(uint8_t address, uint8_t value, EmuTime time);
	void setMode(Mode newMode);

	// public getters for classes interested to show SCC data
	[[nodiscard]] const std::array<std::array<int8_t, 32>, 5>& getWaveData() const { return wave; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;

	struct Event;

	[[nodiscard]] int waveChannel(uint8_t address, bool write) const;
	[[nodiscard]] int8_t currentByte(unsigned channel, unsigned address) const;
	[[nodiscard]] uint8_t readWave(unsigned channel, unsigned address) const;
	void writeWave(unsigned channel, unsigned address, uint8_t value);
	void resetRegisters();
	[[nodiscard]] unsigned effectivePeriod(unsigned channel) const;
	[[nodiscard]] unsigned remaining(unsigned channel) const;
	[[nodiscard]] unsigned advanceCounter(unsigned channel, unsigned clocks);
	[[nodiscard]] bool isLatched(unsigned channel) const;
	[[nodiscard]] uint64_t accessCycle(EmuTime time);
	void queueEvent(unsigned channel, const Event& event);
	void applyEvent(unsigned channel, const Event& event);
	void reload(unsigned channel, bool restart);
	[[nodiscard]] float sample(unsigned channel, uint64_t base);
	[[nodiscard]] float runSample(unsigned channel, uint64_t base);
	void skipBlock(unsigned channel, unsigned num);
	void setDeformReg(uint8_t value);
	void setDeformRegHelper(uint8_t value);
	bool setFreqVol(unsigned address, uint8_t value);
	[[nodiscard]] uint8_t getFreqVol(unsigned address) const;

private:
	static constexpr int CLOCK_FREQ = 3579545;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;

	// Something the CPU did to a channel, taking effect at a cycle of the
	// stream (see accessCycle()).
	struct Event {
		enum Kind : uint8_t { READ, WRITE, RELOAD };
		uint64_t cycle;
		Kind kind;
		uint8_t address; // READ, WRITE: in the channel's RAM
		uint8_t data;    // WRITE: the byte; RELOAD: bit 0 = restart the waveform
	};
	// The serial multiplier of a channel: 8 bits read one per cycle, the
	// product written to the output latch on the cycle after.
	struct Multiplier {
		bool active = false;
		uint8_t bit = 0;  // next bit to read, 8 = product ready
		uint8_t byte = 0; // the bits read so far
		uint8_t vol = 0;  // the volume as latched when it started
	};

	Mode currentMode;

	std::array<std::array<int8_t, 32>, 5> wave;
	std::array<unsigned, 5> counter; // 12-bit frequency counter, counts down
	std::array<unsigned, 5> pos;
	std::array<unsigned, 5> orgPeriod;
	std::array<float, 5> out; // ints stored as floats
	std::array<uint8_t, 5> volume;
	uint8_t ch_enable;

	uint8_t deformValue;
	std::array<bool, 5> rotate;
	std::array<bool, 5> readOnly;
	std::array<int8_t, 2> waveLatch; // channel 4 and 5, see isLatched()

	// The stream position and what is pending on it. Not serialized: a
	// loaded state starts with a clean stream.
	uint64_t streamCycle = 0; // cycles generated so far
	std::array<Multiplier, 5> multiplier;
	std::array<std::vector<Event>, 5> events; // in order of time
	std::array<uint64_t, 5> stealUntil = {}; // the RAM port shows the CPU's byte before this cycle ...
	std::array<uint8_t, 5> stealByte = {};   // ... namely this one
};

SERIALIZE_CLASS_VERSION(SCC, 4);

} // namespace openmsx

#endif
