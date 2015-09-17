#ifndef MSXMULTIIODEVICE_HH
#define MSXMULTIIODEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiIODevice final : public MSXMultiDevice
{
public:
	using Devices = std::vector<MSXDevice*>;

	explicit MSXMultiIODevice(const HardwareConfig& hwConf);
	~MSXMultiIODevice();

	void addDevice(MSXDevice* device);
	void removeDevice(MSXDevice* device);
	Devices& getDevices() { return devices; }

	// MSXDevice
	std::string getName() const override;
	void getNameList(TclObject& result) const override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

private:
	Devices devices; // ordered to get predictable readIO() conflict resolution
};

} // namespace openmsx

#endif
