// $Id$

#ifndef MSXMULTIIODEVICE_HH
#define MSXMULTIIODEVICE_HH

#include <vector>
#include "MSXDevice.hh"

namespace openmsx {

class MSXMultiIODevice : public MSXDevice
{
public:
	typedef std::vector<MSXDevice*> Devices;

	MSXMultiIODevice();
	virtual ~MSXMultiIODevice();

	void addDevice(MSXDevice* device);
	void removeDevice(MSXDevice* device);
	Devices& getDevices();

	// MSXDevice
	virtual void reset(const EmuTime& time);
	virtual void powerDown(const EmuTime& time);
	virtual void powerUp(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	static const XMLElement& getMultiConfig();
	void preCalcName();

	Devices devices;
	std::string name;
};

} // namespace openmsx

#endif
