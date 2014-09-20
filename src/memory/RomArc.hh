#ifndef ROMARC_HH
#define ROMARC_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomArc final : public Rom16kBBlocks
{
public:
	RomArc(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomArc();

	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte offset;
};

} // namspace openmsx

#endif
