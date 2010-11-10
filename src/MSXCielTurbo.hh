// $Id$

#ifndef MSXCIELTURBO_HH
#define MSXCIELTURBO_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class MSXCielTurbo : public MSXDevice
{
public:
	MSXCielTurbo(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXCielTurbo();

	// MSXDevice
	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte lastValue;
};

} // namespace openmsx

#endif
