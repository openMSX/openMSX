// $Id$

#ifndef __SCC_HH__
#define __SCC_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"
#include "Debuggable.hh"

namespace openmsx {

class SCC : private SoundDevice
{
public:
	enum ChipMode {SCC_Real, SCC_Compatible, SCC_plusmode};

	SCC(const string& name, const XMLElement& config, const EmuTime& time,
	    ChipMode mode = SCC_Real);
	virtual ~SCC();

	// interaction with realCartridge
	void reset(const EmuTime& time);
	byte readMemInterface(byte address,const EmuTime& time);
	void writeMemInterface(byte address, byte value, const EmuTime& time);
	void setChipMode(ChipMode newMode);

private:
	// SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void setSampleRate(int sampleRate);
	virtual void setVolume(int maxVolume);
	virtual void updateBuffer(int length, int* buffer);

	class SCCDebuggable : public Debuggable {
	public:
		SCCDebuggable(SCC& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		SCC& parent;
	} sccDebuggable;

	inline void checkMute();
	byte readWave(byte channel, byte address, const EmuTime& time);
	void writeWave(byte channel, byte offset, byte value);
	void setDeformReg(byte value, const EmuTime& time);
	void setFreqVol(byte address, byte value);
	byte getFreqVol(byte address);

	static const int GETA_BITS = 22;
	static const int CLOCK_FREQ = 3579545;
	static const unsigned int SCC_STEP =
		(unsigned)(((unsigned)(1 << 31)) / (CLOCK_FREQ / 2));

	ChipMode currentChipMode;
	int masterVolume;
	unsigned realstep;
	unsigned scctime;

	char wave[5][32];
	int volAdjustedWave[5][32];
	unsigned incr[5];
	unsigned count[5];
	unsigned freq[5];
	byte volume[5];
	byte ch_enable;
	
	byte deformValue;
	Clock<CLOCK_FREQ> deformTimer;
	bool rotate[5];
	bool readOnly[5];
	byte offset[5];

	const string name;
};

} // namespace openmsx

#endif //__SCC_HH__
