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
	explicit GoudaSCSI(const DeviceConfig& config);
	virtual ~GoudaSCSI();

	virtual void reset(EmuTime::param time);

	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte readIO(word port, EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<WD33C93> wd33c93;
};

} // namespace openmsx

#endif
