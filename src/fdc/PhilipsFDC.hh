// $Id$

#ifndef PHILIPSFDC_HH
#define PHILIPSFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class PhilipsFDC : public WD2793BasedFDC
{
public:
	PhilipsFDC(MSXMotherBoard& motherBoard, const DeviceConfig& config);

	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte sideReg;
	byte driveReg;
};

} // namespace openmsx

#endif
