/*
 * This class implements the
 *   backup RAM
 *   bitmap function
 * of the S1985 MSX-engine
 *
 *  TODO explanation
 */

#ifndef S1985_HH
#define S1985_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include "SRAM.hh"

namespace openmsx {

class MSXS1985 final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXS1985(const DeviceConfig& config);
	~MSXS1985() override;

	// MSXDevice
	void reset(EmuTime::param time) override;

	// MSXSwitchedDevice
	[[nodiscard]] byte readSwitchedIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SRAM sram;
	nibble address;
	byte color1;
	byte color2;
	byte pattern;
};
SERIALIZE_CLASS_VERSION(MSXS1985, 2);

} // namespace openmsx

#endif
