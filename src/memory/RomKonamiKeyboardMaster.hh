// $Id$

#ifndef ROMKONAMIKEYBOARDMASTER_HH
#define ROMKONAMIKEYBOARDMASTER_HH

#include "Rom16kBBlocks.hh"

namespace openmsx {

class VLM5030;

class RomKonamiKeyboardMaster : public Rom16kBBlocks
{
public:
	RomKonamiKeyboardMaster(MSXMotherBoard& motherBoard, const XMLElement& config,
	               const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomKonamiKeyboardMaster();

	virtual void reset(const EmuTime& time);

	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);

private:
	const std::auto_ptr<VLM5030> vlm5030;
};

} // namespace openmsx

#endif
