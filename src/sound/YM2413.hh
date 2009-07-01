// $Id$

#ifndef YM2413_HH
#define YM2413_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include <memory>
#include <string>

namespace openmsx {

class YM2413Core;
class YM2413Debuggable;
class MSXMotherBoard;
class XMLElement;

class YM2413 : public SoundDevice, protected Resample
{
public:
	YM2413(MSXMotherBoard& motherBoard, const std::string& name,
	       const XMLElement& config);
	virtual ~YM2413();

	void reset(EmuTime::param time);
	void writeReg(byte reg, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual int getAmplificationFactor() const;
	virtual bool updateBuffer(unsigned length, int* buffer,
		EmuTime::param time, EmuDuration::param sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	const std::auto_ptr<YM2413Core> core;
	const std::auto_ptr<YM2413Debuggable> debuggable;
	friend class YM2413Debuggable;
};

} // namespace openmsx

#endif

