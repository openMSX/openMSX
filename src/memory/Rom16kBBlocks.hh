// $Id$

#ifndef ROM16KBBLOCKS_HH
#define ROM16KBBLOCKS_HH

#include "MSXRom.hh"

namespace openmsx {

class Rom16kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	Rom16kBBlocks(MSXMotherBoard& motherBoard, const XMLElement& config,
	              std::auto_ptr<Rom> rom);

	void setBank(byte region, const byte* adr);
	void setRom(byte region, int block);

private:
	const byte* bank[4];
};

REGISTER_BASE_CLASS(Rom16kBBlocks, "Rom16kBBlocks");

} // namespace openmsx

#endif
