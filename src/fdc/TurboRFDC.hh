// $Id$

#ifndef TURBORFDC_HH
#define TURBORFDC_HH

#include "MSXFDC.hh"
#include <memory>

namespace openmsx {

class MSXCPU;
class TC8566AF;

class TurboRFDC : public MSXFDC
{
public:
	TurboRFDC(MSXMotherBoard& motherBoard, const DeviceConfig& config);
	virtual ~TurboRFDC();

	virtual void reset(EmuTime::param time);

	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte value);

	MSXCPU& cpu;
	const std::auto_ptr<TC8566AF> controller;
	const byte* memory;
	const byte blockMask;
	byte bank;
};

} // namespace openmsx

#endif
