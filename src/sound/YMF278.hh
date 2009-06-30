// $Id$

#ifndef YMF278_HH
#define YMF278_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class YMF278Impl;

class YMF278
{
public:
	YMF278(MSXMotherBoard& motherBoard, const std::string& name,
	       int ramSize, const XMLElement& config);
	~YMF278();
	void clearRam();
	void reset(EmuTime::param time);
	void writeRegOPL4(byte reg, byte data, EmuTime::param time);
	byte readReg(byte reg, EmuTime::param time);
	byte peekReg(byte reg) const;
	byte readStatus(EmuTime::param time);
	byte peekStatus(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<YMF278Impl> pimple;
};

} // namespace openmsx

#endif
