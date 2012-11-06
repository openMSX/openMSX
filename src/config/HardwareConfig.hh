// $Id$

#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "serialize_meta.hh"
#include "serialize_constr.hh"
#include "noncopyable.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXDevice;
class XMLElement;
class FileContext;

class HardwareConfig : private noncopyable
{
public:
	static std::unique_ptr<XMLElement> loadConfig(
		const std::string& filename);

	static std::unique_ptr<HardwareConfig> createMachineConfig(
		MSXMotherBoard& motherBoard, const std::string& machineName);
	static std::unique_ptr<HardwareConfig> createExtensionConfig(
		MSXMotherBoard& motherBoard, const std::string& extensionName);
	static std::unique_ptr<HardwareConfig> createRomConfig(
		MSXMotherBoard& motherBoard, const std::string& romfile,
		const std::string& slotname, const std::vector<std::string>& options);

	HardwareConfig(MSXMotherBoard& motherBoard, const std::string& hwName);
	~HardwareConfig();

	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

	const FileContext& getFileContext() const;
	void setFileContext(std::unique_ptr<FileContext> context);

	const XMLElement& getConfig() const;
	const std::string& getName() const;

	void parseSlots();
	void createDevices();

	/** Checks whether this HardwareConfig can be deleted.
	  * Throws an exception if not.
	  */
	void testRemove() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setConfig(std::unique_ptr<XMLElement> config);
	void load(string_ref path);

	const XMLElement& getDevices() const;
	void createDevices(const XMLElement& elem,
	                   const XMLElement* primary, const XMLElement* secondary);
	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void createExpandedSlot(int ps);
	int getFreePrimarySlot();
	void addDevice(MSXDevice* device);
	void setName(const std::string& proposedName);

	MSXMotherBoard& motherBoard;
	std::string hwName;
	std::string userName;
	std::unique_ptr<XMLElement> config;
	std::unique_ptr<FileContext> context;

	bool externalSlots[4][4];
	bool externalPrimSlots[4];
	bool expandedSlots[4];
	bool allocatedPrimarySlots[4];

	typedef std::vector<MSXDevice*> Devices;
	Devices devices;

	std::string name;

	friend struct SerializeConstructorArgs<HardwareConfig>;
};
SERIALIZE_CLASS_VERSION(HardwareConfig, 2);

template<> struct SerializeConstructorArgs<HardwareConfig>
{
	typedef std::tuple<std::string> type;
	template<typename Archive> void save(
		Archive& ar, const HardwareConfig& config)
	{
		ar.serialize("hwname", config.hwName);
	}
	template<typename Archive> type load(Archive& ar, unsigned /*version*/)
	{
		std::string name;
		ar.serialize("hwname", name);
		return std::make_tuple(name);
	}
};

} // namespace openmsx

#endif
