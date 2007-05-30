// $Id$

#ifndef YM2413_2_HH
#define YM2413_2_HH

#include "YM2413Core.hh"
#include <memory>

namespace openmsx {

// Defined in .cc:
class Global;

class YM2413_2 : public YM2413Core
{
public:
	YM2413_2(MSXMotherBoard& motherBoard, const std::string& name,
	         const XMLElement& config, const EmuTime& time);
	virtual ~YM2413_2();

	void reset(const EmuTime& time);
	void writeReg(byte r, byte v, const EmuTime& time);

private:
	// SoundDevice
	virtual void generateChannels(int** bufs, unsigned num);

	const std::auto_ptr<Global> global;
};

} // namespace openmsx

#endif
