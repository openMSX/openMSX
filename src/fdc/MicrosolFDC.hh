// $Id$

#ifndef MICROSOLFDC_HH
#define MICROSOLFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class MicrosolFDC : public WD2793BasedFDC
{
public:
	explicit MicrosolFDC(const DeviceConfig& config);

	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
