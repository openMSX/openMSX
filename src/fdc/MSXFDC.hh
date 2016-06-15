#ifndef MSXFDC_HH
#define MSXFDC_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include <memory>
#include <string>

namespace openmsx {

class DiskDrive;

class MSXFDC : public MSXDevice
{
public:
	void powerDown(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	const byte* getReadCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	explicit MSXFDC(const DeviceConfig& config, const std::string& romId = "");
	~MSXFDC();

	Rom rom;
	std::unique_ptr<DiskDrive> drives[4];
};

REGISTER_BASE_NAME_HELPER(MSXFDC, "FDC");

} // namespace openmsx

#endif
