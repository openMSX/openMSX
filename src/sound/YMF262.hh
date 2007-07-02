// $Id$

#ifndef YMF262_HH
#define YMF262_HH

#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class EmuTime;
class YMF262Impl;

class YMF262
{
public:
	YMF262(MSXMotherBoard& motherBoard, const std::string& name,
	       const XMLElement& config, const EmuTime& time);
	~YMF262();

	void reset(const EmuTime& time);
	void writeReg(int r, byte v, const EmuTime& time);
	byte readReg(int reg);
	byte peekReg(int reg) const;
	byte readStatus();
	byte peekStatus() const;

private:
	const std::auto_ptr<YMF262Impl> pimple;
};

} // namespace openmsx

#endif
