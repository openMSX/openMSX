// $Id$

#ifndef YM2413_HH
#define YM2413_HH

#include "YM2413Core.hh"
#include <memory>

namespace openmsx {

// Defined in .cc:
namespace YM2413Okazaki {
class Global;
}

class YM2413 : public YM2413Core
{
public:
	YM2413(MSXMotherBoard& motherBoard, const std::string& name,
	       const XMLElement& config, const EmuTime& time);
	virtual ~YM2413();

	void reset(const EmuTime& time);
	void writeReg(byte reg, byte value, const EmuTime& time);

private:
	// SoundDevice
	virtual void generateChannels(int** bufs, unsigned num);

	const std::auto_ptr<YM2413Okazaki::Global> global;
};

} // namespace openmsx

#endif
