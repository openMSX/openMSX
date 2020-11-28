#ifndef MSXFDC_HH
#define MSXFDC_HH

#include "MSXDevice.hh"
#include "DiskDrive.hh"
#include <memory>
#include <string>

namespace openmsx {

class Rom;

class MSXFDC : public MSXDevice
{
public:
	void powerDown(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	explicit MSXFDC(const DeviceConfig& config, const std::string& romId = {},
	                bool needROM = true,
	                DiskDrive::TrackMode trackMode = DiskDrive::TrackMode::NORMAL);
	~MSXFDC() override;

protected:
	std::unique_ptr<Rom> rom;
	std::unique_ptr<DiskDrive> drives[4];
};

REGISTER_BASE_NAME_HELPER(MSXFDC, "FDC");

} // namespace openmsx

#endif
