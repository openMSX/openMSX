// $Id$

#ifndef SCC_HH
#define SCC_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "Clock.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class SCCDebuggable;

class SCC : public SoundDevice, private Resample<1>
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

	// Resample
	virtual void generateInput(float* buffer, unsigned num);

	inline void checkMute();
	byte readWave(byte channel, byte address, const EmuTime& time);
	void writeWave(byte channel, byte offset, byte value);
	void setDeformReg(byte value, const EmuTime& time);
	void setFreqVol(byte address, byte value);
	byte getFreqVol(byte address);

	int filter4(int in1, int in2, int in3, int in4);

	static const int CLOCK_FREQ = 3579545;
	static const unsigned int SCC_STEP =
		(unsigned)(((unsigned)(1 << 31)) / (CLOCK_FREQ / 2));

	ChipMode currentChipMode;
	int masterVolume;
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

	// previous input values for FIR filter
	int in[48];

	friend class SCCDebuggable;
	const std::auto_ptr<SCCDebuggable> debuggable;
};

} // namespace openmsx

#endif
