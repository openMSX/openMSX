#ifndef MSXMULTIIODEVICE_HH
#define MSXMULTIIODEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiIODevice final : public MSXMultiDevice
{
public:
	typedef std::vector<MSXDevice*> Devices;

	explicit MSXMultiIODevice(const HardwareConfig& hwConf);
	~MSXMultiIODevice();

	void addDevice(MSXDevice* device);
	void removeDevice(MSXDevice* device);
	Devices& getDevices() { return devices; }

	// MSXDevice
	virtual std::string getName() const;
	virtual void getNameList(TclObject& result) const;
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

private:
	Devices devices;
};

} // namespace openmsx

#endif
