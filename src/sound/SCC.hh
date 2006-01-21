// $Id$

#ifndef SCC_HH
#define SCC_HH

#include "SoundDevice.hh"
#include "Clock.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class SCCDebuggable;

class SCC : public SoundDevice
{
public:
	enum ChipMode {SCC_Real, SCC_Compatible, SCC_plusmode};

	SCC(MSXMotherBoard& motherBoard, const std::string& name,
	    const XMLElement& config, const EmuTime& time,
	    ChipMode mode = SCC_Real);
	virtual ~SCC();

	// interaction with realCartridge
	void reset(const EmuTime& time);
	byte readMemInterface(byte address,const EmuTime& time);
	void writeMemInterface(byte address, byte value, const EmuTime& time);
	void setChipMode(ChipMode newMode);

private:
	// SoundDevice
	virtual void setSampleRate(int sampleRate);
	virtual void setVolume(int maxVolume);
	virtual void updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	inline void checkMute();
	byte readWave(byte channel, byte address, const EmuTime& time);
	void writeWave(byte channel, byte offset, byte value);
	void setDeformReg(byte value, const EmuTime& time);
	void setFreqVol(byte address, byte value);
	byte getFreqVol(byte address);

	int filter(int input);

	static const int CLOCK_FREQ = 3579545;
	static const unsigned int SCC_STEP =
		(unsigned)(((unsigned)(1 << 31)) / (CLOCK_FREQ / 2));

	ChipMode currentChipMode;
	int masterVolume;
	unsigned baseStep;
	unsigned nbSamples;

	signed char wave[5][32];
	signed char volAdjustedWave[5][32];
	unsigned incr[5];
	unsigned count[5];
	unsigned pos[5];
	unsigned freq[5];
	int out[5];
	byte volume[5];
	byte ch_enable;

	byte deformValue;
	Clock<CLOCK_FREQ> deformTimer;
	bool rotate[5];
	bool readOnly[5];

	int in[5], inHp[3], outHp[3];

	friend class SCCDebuggable;
	const std::auto_ptr<SCCDebuggable> debuggable;
};

} // namespace openmsx

#endif
