// $Id$

#ifndef MSXMULTIIODEVICE_HH
#define MSXMULTIIODEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiIODevice : public MSXMultiDevice
{
public:
	typedef std::vector<MSXDevice*> Devices;

	explicit MSXMultiIODevice(MSXMotherBoard& motherboard);
	virtual ~MSXMultiIODevice();

	void addDevice(MSXDevice* device);
	void removeDevice(MSXDevice* device);
	Devices& getDevices();

	// MSXDevice
	virtual std::string getName() const;
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

private:
	Devices devices;
};

} // namespace openmsx

#endif
