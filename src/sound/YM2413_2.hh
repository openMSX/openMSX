// $Id$

#ifndef YM2413_2_HH
#define YM2413_2_HH

#include "YM2413Core.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "FixedPoint.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class EmuTime;
class MSXMotherBoard;
class YM2413_2Debuggable;

// Defined in .cc:
class Globals;

class YM2413_2 : public YM2413Core, public SoundDevice, private Resample<1>
{
public:
	YM2413_2(MSXMotherBoard& motherBoard, const std::string& name,
	         const XMLElement& config, const EmuTime& time);
	virtual ~YM2413_2();

	void reset(const EmuTime& time);
	void writeReg(byte r, byte v, const EmuTime& time);

private:
	void checkMute();

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual void updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual void generateInput(float* buffer, unsigned num);

	friend class YM2413_2Debuggable;
	const std::auto_ptr<YM2413_2Debuggable> debuggable;

	const std::auto_ptr<Globals> globals;

	byte reg[0x40];
};

} // namespace openmsx

#endif
