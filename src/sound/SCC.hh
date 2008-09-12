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

class SCC : public SoundDevice, private Resample
{
public:
	enum ChipMode {SCC_Real, SCC_Compatible, SCC_plusmode};

	SCC(MSXMotherBoard& motherBoard, const std::string& name,
	    const XMLElement& config, const EmuTime& time,
	    ChipMode mode = SCC_Real);
	virtual ~SCC();

	// interaction with realCartridge
	void reset(const EmuTime& time);
	byte readMem(byte address,const EmuTime& time);
	byte peekMem(byte address,const EmuTime& time) const;
	void writeMem(byte address, byte value, const EmuTime& time);
	void setChipMode(ChipMode newMode);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	virtual int getAmplificationFactor() const;
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	inline int adjust(signed char wav, byte vol);
	byte readWave(unsigned channel, unsigned address, const EmuTime& time) const;
	void writeWave(unsigned channel, unsigned offset, byte value);
	void setDeformReg(byte value, const EmuTime& time);
	void setDeformRegHelper(byte value);
	void setFreqVol(unsigned address, byte value, const EmuTime& time);
	byte getFreqVol(unsigned address) const;

	static const int CLOCK_FREQ = 3579545;

	friend class SCCDebuggable;
	const std::auto_ptr<SCCDebuggable> debuggable;

	Clock<CLOCK_FREQ> deformTimer;
	ChipMode currentChipMode;

	signed char wave[5][32];
	int volAdjustedWave[5][32];
	unsigned incr[5];
	unsigned count[5];
	unsigned pos[5];
	unsigned period[5];
	unsigned orgPeriod[5];
	int out[5];
	byte volume[5];
	byte ch_enable;

	byte deformValue;
	bool rotate[5];
	bool readOnly[5];
};

} // namespace openmsx

#endif
