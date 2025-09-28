#ifndef MSXFDC_HH
#define MSXFDC_HH

#include "DiskDrive.hh"

#include "MSXDevice.hh"
#include "Rom.hh"

#include <array>
#include <memory>
#include <optional>
#include <string>

namespace openmsx {

class MSXFDC : public MSXDevice
{
public:
	void powerDown(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;

	void getExtraDeviceInfo(TclObject& result) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	explicit MSXFDC(DeviceConfig& config, const std::string& romId = {},
	                bool needROM = true,
	                DiskDrive::TrackMode trackMode = DiskDrive::TrackMode::NORMAL);

	void parseRomVisibility(DeviceConfig& config, unsigned defaultBase, unsigned defaultSize);

protected:
	std::optional<Rom> rom;
	uint16_t romVisibilityStart = 0;
	uint16_t romVisibilityLast = 0xFFFF; // so, inclusive

	std::array<std::unique_ptr<DiskDrive>, 4> drives;
};

REGISTER_BASE_NAME_HELPER(MSXFDC, "FDC");

} // namespace openmsx

#endif
