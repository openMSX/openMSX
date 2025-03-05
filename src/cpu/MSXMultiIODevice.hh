#ifndef MSXMULTIIODEVICE_HH
#define MSXMULTIIODEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiIODevice final : public MSXMultiDevice
{
public:
	using Devices = std::vector<MSXDevice*>;

	explicit MSXMultiIODevice(HardwareConfig& hwConf);
	~MSXMultiIODevice() override;

	void addDevice(MSXDevice* device);
	void removeDevice(MSXDevice* device);
	[[nodiscard]] Devices& getDevices() { return devices; }

	// MSXDevice
	[[nodiscard]] const std::string& getName() const override;
	void getNameList(TclObject& result) const override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime::param time) override;

private:
	Devices devices; // ordered to get predictable readIO() conflict resolution
};

} // namespace openmsx

#endif
