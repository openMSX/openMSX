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
	       const XMLElement& config);
	~YMF262();

	void reset(const EmuTime& time);
	void writeReg(unsigned r, byte v, const EmuTime& time);
	byte readReg(unsigned reg);
	byte peekReg(unsigned reg) const;
	byte readStatus();
	byte peekStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<YMF262Impl> pimple;
};

} // namespace openmsx

#endif
