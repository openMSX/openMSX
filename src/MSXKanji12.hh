#ifndef MSXKANJI12_HH
#define MSXKANJI12_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXKanji12 final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXKanji12(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;

	// MSXSwitchedDevice
	[[nodiscard]] byte readSwitchedIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	unsigned address;
};

} // namespace openmsx

#endif
