// $Id$

#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "XMLElement.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXDevice;

class HardwareConfig
{
public:
	HardwareConfig(MSXMotherBoard& motherBoard);
	virtual ~HardwareConfig();

	const XMLElement& getConfig() const;
	void setConfig(std::auto_ptr<XMLElement> config);
	void load(const std::string& path, const std::string& hwName);
	
	void reserveSlot(int slot);

	void parseSlots();
	void createDevices();

private:
	virtual const XMLElement& getDevices() const = 0;

	void createDevices(const XMLElement& elem);
	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void createExpandedSlot(int ps);
	int getFreePrimarySlot();
	void addDevice(MSXDevice* device);

	MSXMotherBoard& motherBoard;
	std::auto_ptr<XMLElement> config;

	int reservedSlot;
	bool externalSlots[4][4];
	bool externalPrimSlots[4];
	bool expandedSlots[4];
	int allocatedPrimarySlots[4];

	typedef std::vector<MSXDevice*> Devices;
	Devices devices;
};

} // namespace openmsx

#endif
