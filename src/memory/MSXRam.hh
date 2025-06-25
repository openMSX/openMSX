#ifndef MSXRAM_HH
#define MSXRAM_HH

#include "MSXDevice.hh"
#include "CheckedRam.hh"
#include <optional>

namespace openmsx {

class MSXRam final : public MSXDevice
{
public:
	explicit MSXRam(const DeviceConfig& config);

	void powerUp(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void init() override;
	[[nodiscard]] inline unsigned translate(unsigned address) const;

private:
	/*const*/ unsigned base;
	/*const*/ unsigned size;
	/*const*/ std::optional<CheckedRam> checkedRam;
};

} // namespace openmsx

#endif
