// $Id$

#ifndef ROMARC_HH
#define ROMARC_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomArc : public Rom16kBBlocks
{
public:
	RomArc(MSXMotherBoard& motherBoard, const XMLElement& config,
	       std::auto_ptr<Rom> rom);
	virtual ~RomArc();

	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte offset;
};

} // namspace openmsx

#endif
