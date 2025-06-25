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
#include <memory>

namespace openmsx {

using uint4_t = uint8_t;

class SRAM;

class MSXS1985 final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXS1985(const DeviceConfig& config);
	~MSXS1985() override;

	// MSXDevice
	void reset(EmuTime time) override;

	// MSXSwitchedDevice
	[[nodiscard]] uint8_t readSwitchedIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekSwitchedIO(uint16_t port, EmuTime time) const override;
	void writeSwitchedIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::unique_ptr<SRAM> sram;
	uint4_t address;
	uint8_t color1;
	uint8_t color2;
	uint8_t pattern;
};
SERIALIZE_CLASS_VERSION(MSXS1985, 2);

} // namespace openmsx

#endif
