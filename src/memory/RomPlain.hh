// $Id$

#ifndef ROMPLAIN_HH
#define ROMPLAIN_HH

#include "Rom8kBBlocks.hh"
#include "serialize.hh"
#include "ref.hh"

namespace openmsx {

class XMLElement;

class RomPlain : public Rom8kBBlocks
{
public:
	enum MirrorType { MIRRORED, NOT_MIRRORED };

	RomPlain(MSXMotherBoard& motherBoard, const XMLElement& config,
	         std::auto_ptr<Rom> rom, MirrorType mirrored, int start = -1);

private:
	void guessHelper(unsigned offset, int* pages);
	unsigned guessLocation(unsigned windowBase, unsigned windowSize);
};

REGISTER_POLYMORPHIC_CLASS_2(MSXDevice, RomPlain, "RomPlain",
                             reference_wrapper<MSXMotherBoard>,
                             reference_wrapper<HardwareConfig>);
template<> struct Creator<RomPlain> : MSXDeviceCreator<RomPlain> {};
template<> struct SerializeConstructorArgs<RomPlain>
	: SerializeConstructorArgs<MSXDevice> {};

} // namespace openmsx

#endif
