// $Id$

#ifndef GOUDASCSI_HH
#define GOUDASCSI_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class WD33C93;
class Rom;

class GoudaSCSI : public MSXDevice
{
public:
	GoudaSCSI(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~GoudaSCSI();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte readIO(word port, const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;

private:
	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<WD33C93> wd33c93;
};

} // namespace openmsx

#endif
