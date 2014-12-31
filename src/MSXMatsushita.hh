#ifndef MSXMATSUSHITA_HH
#define MSXMATSUSHITA_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include "FirmwareSwitch.hh"
#include "SRAM.hh"

namespace openmsx {

class MSXMatsushita final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXMatsushita(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;

	// MSXSwitchedDevice
	byte readSwitchedIO(word port, EmuTime::param time) override;
	byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	FirmwareSwitch firmwareSwitch;
	SRAM sram;
	word address;
	nibble color1, color2;
	byte pattern;
};

} // namespace openmsx

#endif
