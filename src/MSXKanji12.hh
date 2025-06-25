#ifndef MSXKANJI12_HH
#define MSXKANJI12_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXKanji12 final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXKanji12(DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime time) override;

	// MSXSwitchedDevice
	[[nodiscard]] byte readSwitchedIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekSwitchedIO(uint16_t port, EmuTime time) const override;
	void writeSwitchedIO(uint16_t port, byte value, EmuTime time) override;

	void getExtraDeviceInfo(TclObject& result) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	unsigned address;
};

} // namespace openmsx

#endif
