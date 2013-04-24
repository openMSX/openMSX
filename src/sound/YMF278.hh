#ifndef YMF278_HH
#define YMF278_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class DeviceConfig;

class YMF278
{
public:
	YMF278(const std::string& name, int ramSize,
	       const DeviceConfig& config);
	~YMF278();
	void clearRam();
	void reset(EmuTime::param time);
	void writeReg(byte reg, byte data, EmuTime::param time);
	byte readReg(byte reg);
	byte peekReg(byte reg) const;

	byte readMem(unsigned address) const;
	void writeMem(unsigned address, byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Impl;
	const std::unique_ptr<Impl> pimpl;
};
SERIALIZE_CLASS_VERSION(YMF278, 3);

} // namespace openmsx

#endif
