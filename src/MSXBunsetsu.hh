// $Id$

#ifndef MSXBUNSETSU_HH
#define MSXBUNSETSU_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXBunsetsu : public MSXDevice
{
public:
	MSXBunsetsu(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXBunsetsu();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::auto_ptr<Rom> bunsetsuRom;
	std::auto_ptr<Rom> jisyoRom;
	int jisyoAddress;
};

REGISTER_MSXDEVICE(MSXBunsetsu, "Bunsetsu");

} // namespace openmsx

#endif
