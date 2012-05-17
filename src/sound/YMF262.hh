// $Id$

#ifndef YMF262_HH
#define YMF262_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class DeviceConfig;

class YMF262
{
public:
	YMF262(MSXMotherBoard& motherBoard, const std::string& name,
	       const DeviceConfig& config, bool isYMF278);
	~YMF262();

	void reset(EmuTime::param time);
	void writeReg   (unsigned r, byte v, EmuTime::param time);
	void writeReg512(unsigned r, byte v, EmuTime::param time);
	byte readReg(unsigned reg);
	byte peekReg(unsigned reg) const;
	byte readStatus();
	byte peekStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Impl;
	const std::auto_ptr<Impl> pimpl;
};
SERIALIZE_CLASS_VERSION(YMF262, 2);

} // namespace openmsx

#endif
