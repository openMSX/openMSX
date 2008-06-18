// $Id$

#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "noncopyable.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXDevice;
class XMLElement;

class HardwareConfig : private noncopyable
{
public:
	HardwareConfig(MSXMotherBoard& motherBoard, const std::string& hwName);
	virtual ~HardwareConfig();

	const XMLElement& getConfig() const;

	void parseSlots();
	void createDevices();

	/** Checks whether this HardwareConfig can be deleted.
	  * Throws an exception if not.
	  */
	void testRemove() const;

	static std::auto_ptr<XMLElement> loadConfig(
		const std::string& path, const std::string& hwName,
		const std::string& userName);

protected:
	void setConfig(std::auto_ptr<XMLElement> config);
	void load(const std::string& path);
	MSXMotherBoard& getMotherBoard();

private:
	const XMLElement& getDevices() const;
	void createDevices(const XMLElement& elem);
	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void createExpandedSlot(int ps);
	int getFreePrimarySlot();
	void addDevice(MSXDevice* device);

	MSXMotherBoard& motherBoard;
	std::string hwName;
	std::string userName;
	std::auto_ptr<XMLElement> config;

	bool externalSlots[4][4];
	bool externalPrimSlots[4];
	bool expandedSlots[4];
	int allocatedPrimarySlots[4];

	typedef std::vector<MSXDevice*> Devices;
	Devices devices;
};

} // namespace openmsx

#endif
