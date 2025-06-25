#ifndef ROMALALAMIAH30IN1_HH
#define ROMALALAMIAH30IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomAlAlamiah30in1 final : public Rom16kBBlocks
{
public:
	RomAlAlamiah30in1(const DeviceConfig& config, Rom&& rom);
	~RomAlAlamiah30in1() override;

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool mapperLocked = false;
};

} // namespace openmsx

#endif
