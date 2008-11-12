// $Id$

#ifndef YM2413CORE_HH
#define YM2413CORE_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <memory>
#include <string>

namespace openmsx {

class MSXMotherBoard;

// Defined in .cc:
class YM2413Debuggable;

class YM2413Core : public SoundDevice, protected Resample
{
public:
	// Input clock frequency.
	// An output sample is generated every 72 cycles
	static const int CLOCK_FREQ = 3579545;

	YM2413Core(MSXMotherBoard& motherBoard, const std::string& name);
	virtual ~YM2413Core();
	virtual void writeReg(byte reg, byte value, EmuTime::param time) = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	byte reg[0x40];

private:
	// SoundDevice:
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num) = 0;
	virtual bool updateBuffer(unsigned length, int* buffer,
		EmuTime::param time, EmuDuration::param sampDur);

	// Resample:
	virtual bool generateInput(int* buffer, unsigned num);

	friend class YM2413Debuggable;
	const std::auto_ptr<YM2413Debuggable> debuggable;
};
REGISTER_BASE_NAME_HELPER(YM2413Core, "YM2413Core");

} // namespace openmsx

#endif
