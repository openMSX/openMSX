#ifndef MSXRAM_HH
#define MSXRAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class CheckedRam;

class MSXRam final : public MSXDevice
{
public:
	explicit MSXRam(const DeviceConfig& config);
	~MSXRam() override;

	void powerUp(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void init() override;
	[[nodiscard]] inline unsigned translate(unsigned address) const;

private:
	/*const*/ unsigned base;
	/*const*/ unsigned size;
	/*const*/ std::unique_ptr<CheckedRam> checkedRam;
};

} // namespace openmsx

#endif
