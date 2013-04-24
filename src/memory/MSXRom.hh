#ifndef MSXROM_HH
#define MSXROM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXRom : public MSXDevice
{
public:
	virtual ~MSXRom();

	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	virtual void getExtraDeviceInfo(TclObject& result) const;

protected:
	MSXRom(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	const std::unique_ptr<Rom> rom;
};

} // namespace openmsx

#endif
