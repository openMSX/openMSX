// $Id$

#ifndef ROMMAJUTSUSHI_HH
#define ROMMAJUTSUSHI_HH

#include "RomKonami.hh"

namespace openmsx {

class DACSound8U;

class RomMajutsushi : public RomKonami
{
public:
	RomMajutsushi(MSXMotherBoard& motherBoard, const XMLElement& config,
	              std::auto_ptr<Rom> rom);
	virtual ~RomMajutsushi();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<DACSound8U> dac;
};

REGISTER_MSXDEVICE(RomMajutsushi, "RomMajutsushi");

} // namespace openmsx

#endif
