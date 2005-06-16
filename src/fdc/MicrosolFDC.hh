// $Id$

#ifndef MICROSOLFDC_HH
#define MICROSOLFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class MicrosolFDC : public WD2793BasedFDC
{
public:
	MicrosolFDC(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time);
	virtual ~MicrosolFDC();

	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	byte driveD4;
};

} // namespace openmsx

#endif
