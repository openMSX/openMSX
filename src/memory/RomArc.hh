#ifndef ROMARC_HH
#define ROMARC_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomArc final : public Rom16kBBlocks
{
public:
	RomArc(const DeviceConfig& config, Rom&& rom);
	~RomArc() override;

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte offset;
};

} // namespace openmsx

#endif
