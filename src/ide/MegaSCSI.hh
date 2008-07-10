// $Id$

#ifndef MEGASCSI_HH
#define MEGASCSI_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class MB89352;
class SRAM;

class MegaSCSI : public MSXDevice
{
public:
	MegaSCSI(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MegaSCSI();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual byte peekmem(word address, const EmuTime& time) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setSRAM(unsigned region, byte block);

	std::auto_ptr<SRAM> sram;
	const std::auto_ptr<MB89352> mb89352;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // SPC block mapped in this region?
	byte blockMask;
};

REGISTER_MSXDEVICE(MegaSCSI, "MegaSCSI");

} // namespace openmsx

#endif
