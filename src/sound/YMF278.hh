// $Id$

#ifndef YMF278_HH
#define YMF278_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
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
	void writeReg(byte reg, byte data, EmuTime::param time);
	byte readReg(byte reg);
	byte peekReg(byte reg) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<YMF278Impl> pimple;
};
SERIALIZE_CLASS_VERSION(YMF278, 2);

} // namespace openmsx

#endif
