#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "XMLElement.hh"
#include "FileContext.hh"
#include "serialize_meta.hh"
#include "serialize_constr.hh"
#include "array_ref.hh"
#include "string_ref.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXDevice;
class TclObject;

class HardwareConfig
{
public:
	HardwareConfig(const HardwareConfig&) = delete;
	HardwareConfig& operator=(const HardwareConfig&) = delete;

	static XMLElement loadConfig(string_ref type, string_ref name);

	static std::unique_ptr<HardwareConfig> createMachineConfig(
		MSXMotherBoard& motherBoard, const std::string& machineName);
	static std::unique_ptr<HardwareConfig> createExtensionConfig(
		MSXMotherBoard& motherBoard, string_ref extensionName, string_ref slotname);
	static std::unique_ptr<HardwareConfig> createRomConfig(
		MSXMotherBoard& motherBoard, string_ref romfile,
		string_ref slotname, array_ref<TclObject> options);

	HardwareConfig(MSXMotherBoard& motherBoard, std::string hwName);
	~HardwareConfig();

	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

	const FileContext& getFileContext() const { return context; }
	void setFileContext(FileContext&& ctxt) { context = std::move(ctxt); }

	const XMLElement& getConfig() const { return config; }
	const std::string& getName() const { return name; }

	void parseSlots();
	void createDevices();

	/** Checks whether this HardwareConfig can be deleted.
	  * Throws an exception if not.
	  */
	void testRemove() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	static std::string getFilename(string_ref type, string_ref name);
	static XMLElement loadConfig(const std::string& filename);
	void setConfig(XMLElement config_) { config = std::move(config_); }
	void load(string_ref type);

	const XMLElement& getDevices() const;
	void createDevices(const XMLElement& elem,
	                   const XMLElement* primary, const XMLElement* secondary);
	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void createExpandedSlot(int ps);
	int getAnyFreePrimarySlot();
	int getSpecificFreePrimarySlot(unsigned slot);
	void addDevice(std::unique_ptr<MSXDevice> device);
	void setName(string_ref proposedName);
	void setSlot(string_ref slotname);

	MSXMotherBoard& motherBoard;
	std::string hwName;
	std::string userName;
	XMLElement config;
	FileContext context;

	bool externalSlots[4][4];
	bool externalPrimSlots[4];
	bool expandedSlots[4];
	bool allocatedPrimarySlots[4];

	std::vector<std::unique_ptr<MSXDevice>> devices;

	std::string name;

	friend struct SerializeConstructorArgs<HardwareConfig>;
};
SERIALIZE_CLASS_VERSION(HardwareConfig, 4);

template<> struct SerializeConstructorArgs<HardwareConfig>
{
	using type = std::tuple<std::string>;
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
