// $Id$

#ifndef YM2413_HH
#define YM2413_HH

#include "YM2413Core.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class EmuTime;
class MSXMotherBoard;

// Defined in .cc:
class YM2413Debuggable;
namespace YM2413Okazaki {
class Global;
}

class YM2413 : public YM2413Core, public SoundDevice, private Resample
{
public:
	YM2413(MSXMotherBoard& motherBoard, const std::string& name,
	       const XMLElement& config, const EmuTime& time);
	virtual ~YM2413();

	void reset(const EmuTime& time);
	void writeReg(byte reg, byte value, const EmuTime& time);

private:
	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	friend class YM2413Debuggable;
	const std::auto_ptr<YM2413Debuggable> debuggable;

	const std::auto_ptr<YM2413Okazaki::Global> global;

	byte reg[0x40];
};

} // namespace openmsx

#endif
