// $Id$

#ifndef YM2413_HH
#define YM2413_HH

#include "YM2413Interface.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

// Defined in .cc:
namespace YM2413Okazaki {
class Global;
}
class MSXMotherBoard;
class XMLElement;

class YM2413 : public YM2413Interface
{
public:
	YM2413(MSXMotherBoard& motherBoard, const std::string& name,
	       const XMLElement& config, EmuTime::param time);
	virtual ~YM2413();

	virtual void reset(EmuTime::param time);
	virtual void writeReg(byte reg, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<YM2413Okazaki::Global> global;
};

} // namespace openmsx

#endif
