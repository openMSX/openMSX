// $Id$

#ifndef NATIONALFDC_HH
#define NATIONALFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class NationalFDC : public WD2793BasedFDC
{
public:
	explicit NationalFDC(const DeviceConfig& config);

	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
