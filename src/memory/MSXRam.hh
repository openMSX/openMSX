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

	void powerUp(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;

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
