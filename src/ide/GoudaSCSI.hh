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
	GoudaSCSI(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time);
	virtual ~GoudaSCSI();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);

private:
	const std::auto_ptr<Rom> rom;
	std::auto_ptr<WD33C93> wd33c93;
};

} // namespace openmsx

#endif
