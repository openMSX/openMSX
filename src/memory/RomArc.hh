#ifndef ROMARC_HH
#define ROMARC_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomArc final : public Rom16kBBlocks
{
public:
	RomArc(const DeviceConfig& config, Rom&& rom);
	~RomArc() override;

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	byte offset;
};

} // namspace openmsx

#endif
