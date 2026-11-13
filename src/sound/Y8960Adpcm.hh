#ifndef Y8960ADPCM_HH
#define Y8960ADPCM_HH

#include "Clock.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"

#include <cstdint>

namespace openmsx {

class DeviceConfig;
class Y8960AdpcmMemory;
class Y8960OPL2;

/** The ADPCM-B block of the Y8960's OPL2EX.
  *
  * A copy of Y8950Adpcm, which the Y8960 cannot use as it stands: the
  * sample memory is a device of its own (Y8960AdpcmMemory) rather than a
  * member of the block. PCM_BSY also follows the Y8960, which drops it at
  * the end of a one-shot sample.
  */
class Y8960Adpcm final : public Schedulable
{
public:
	Y8960Adpcm(Y8960OPL2& opl2, const DeviceConfig& config,
	           Y8960AdpcmMemory& memory);

	void clearRam();
	void reset(EmuTime time);
	[[nodiscard]] bool isMuted() const;
	void writeReg(uint8_t rg, uint8_t data, EmuTime time);
	[[nodiscard]] uint8_t readReg(uint8_t rg, EmuTime time);
	[[nodiscard]] uint8_t peekReg(uint8_t rg, EmuTime time) const;
	[[nodiscard]] int calcSample();
	void sync(EmuTime time);
	void resetStatus();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// This data is updated while playing
	struct PlayData {
		unsigned memPtr;
		unsigned nowStep;
		int out;
		int output;
		int diff;
		int nextLeveling;
		int sampleStep;
		uint8_t adpcm_data;
	};

	// Schedulable
	void executeUntil(EmuTime time) override;

	void schedule();
	void restart(PlayData& pd) const;

	[[nodiscard]] bool isPlaying() const;
	void writeData(uint8_t data);
	[[nodiscard]] uint8_t peekReg(uint8_t rg) const;
	[[nodiscard]] uint8_t readData();
	[[nodiscard]] uint8_t peekData() const;
	void writeMemory(unsigned memPtr, uint8_t value);
	[[nodiscard]] uint8_t readMemory(unsigned memPtr) const;
	[[nodiscard]] int calcSample(bool doEmu);

private:
	Y8960OPL2& opl2;
	Y8960AdpcmMemory& memory;

	// copy/pasted from Y8960OPL2.hh
	static constexpr int CLOCK_FREQ     = 3579545;
	static constexpr int CLOCK_FREQ_DIV = 72;
	Clock<CLOCK_FREQ, CLOCK_FREQ_DIV> clock;

	PlayData emu; // used for emulator behaviour (read back of sample data)
	PlayData aud; // used by audio generation thread

	unsigned startAddr;
	unsigned stopAddr;
	unsigned addrMask;
	int volume = 0;
	int volumeWStep;
	int readDelay;
	int delta;
	uint8_t reg7;
	uint8_t reg15;
	bool romBank = false;
};
SERIALIZE_CLASS_VERSION(Y8960Adpcm, 2);

} // namespace openmsx

#endif
