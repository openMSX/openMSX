#ifndef Y8950ADPCM_HH
#define Y8950ADPCM_HH

#include "TrackedRam.hh"
#include "Schedulable.hh"
#include "Clock.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"

namespace openmsx {

class DeviceConfig;
class Y8950;

class Y8950Adpcm final : public Schedulable
{
public:
	Y8950Adpcm(Y8950& y8950, const DeviceConfig& config,
	           const std::string& name, unsigned sampleRam);

	void clearRam();
	void reset(EmuTime::param time);
	bool isMuted() const;
	void writeReg(byte rg, byte data, EmuTime::param time);
	byte readReg(byte rg, EmuTime::param time);
	byte peekReg(byte rg, EmuTime::param time) const;
	int calcSample();
	void sync(EmuTime::param time);
	void resetStatus();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// This data is updated while playing
	struct PlayData {
		unsigned memPntr;
		unsigned nowStep;
		int out;
		int output;
		int diff;
		int nextLeveling;
		int sampleStep;
		byte adpcm_data;
	};

	// Schedulable
	void executeUntil(EmuTime::param time) override;

	void schedule();
	void restart(PlayData& pd);

	bool isPlaying() const;
	void writeData(byte data);
	byte peekReg(byte rg) const;
	byte readData();
	byte peekData() const;
	void writeMemory(unsigned memPntr, byte value);
	byte readMemory(unsigned memPntr) const;
	int calcSample(bool doEmu);

	Y8950& y8950;
	TrackedRam ram;

	// copy/pasted from Y8950.hh
	static constexpr int CLOCK_FREQ     = 3579545;
	static constexpr int CLOCK_FREQ_DIV = 72;
	Clock<CLOCK_FREQ, CLOCK_FREQ_DIV> clock;

	PlayData emu; // used for emulator behaviour (read back of sample data)
	PlayData aud; // used by audio generation thread

	unsigned startAddr;
	unsigned stopAddr;
	unsigned addrMask;
	int volume;
	int volumeWStep;
	int readDelay;
	int delta;
	byte reg7;
	byte reg15;
	bool romBank;
};
SERIALIZE_CLASS_VERSION(Y8950Adpcm, 2);

} // namespace openmsx

#endif
