// $Id$

#ifndef YMF278_HH
#define YMF278_HH

#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class EmuTime;
class YMF278Impl;

class YMF278
{
public:
	YMF278(MSXMotherBoard& motherBoard, const std::string& name,
	       int ramSize, const XMLElement& config, const EmuTime& time);
	~YMF278();
	void reset(const EmuTime& time);
	void writeRegOPL4(byte reg, byte data, const EmuTime& time);
	byte readReg(byte reg, const EmuTime& time);
	byte peekReg(byte reg) const;
	byte readStatus(const EmuTime& time);
	byte peekStatus(const EmuTime& time) const;

private:
	const std::auto_ptr<YMF278Impl> pimple;
};

} // namespace openmsx

#endif
