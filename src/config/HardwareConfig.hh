#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "XMLElement.hh"
#include "FileContext.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include "serialize_constr.hh"
#include "span.hh"
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXDevice;
class TclObject;

class HardwareConfig
{
public:
	enum class Type {
		MACHINE,
		EXTENSION,
		ROM
	};

	HardwareConfig(const HardwareConfig&) = delete;
	HardwareConfig& operator=(const HardwareConfig&) = delete;

	static void loadConfig(XMLDocument& doc, std::string_view type, std::string_view name);

	[[nodiscard]] static std::unique_ptr<HardwareConfig> createMachineConfig(
		MSXMotherBoard& motherBoard, std::string machineName);
	[[nodiscard]] static std::unique_ptr<HardwareConfig> createExtensionConfig(
		MSXMotherBoard& motherBoard, std::string extensionName,
		std::string_view slotname);
	[[nodiscard]] static std::unique_ptr<HardwareConfig> createRomConfig(
		MSXMotherBoard& motherBoard, std::string romfile,
		std::string slotname, span<const TclObject> options);

	HardwareConfig(MSXMotherBoard& motherBoard, std::string hwName);
	~HardwareConfig();

	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

	[[nodiscard]] const FileContext& getFileContext() const { return context; }
	void setFileContext(FileContext&& ctxt) { context = std::move(ctxt); }

	[[nodiscard]] XMLDocument& getXMLDocument() { return config; }
	[[nodiscard]] const XMLElement& getConfig() const { return *config.getRoot(); }
	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] const std::string& getConfigName() const { return hwName; }
	[[nodiscard]] const XMLElement& getDevicesElem() const;
	[[nodiscard]] Type getType() const { return type; }

	/** Parses a slot mapping.
	  * Returns the slot selection: two bits per page for the slot to be
	  * selected in that page, like MSX port 0xA8.
	  */
	[[nodiscard]] byte parseSlotMap() const;

	void parseSlots();
	void createDevices();

	[[nodiscard]] const auto& getDevices() const { return devices; };

	/** Checks whether this HardwareConfig can be deleted.
	  * Throws an exception if not.
	  */
	void testRemove() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setConfig(XMLElement* root) { config.setRoot(root); }
	void load(std::string_view type);

	void createDevices(const XMLElement& elem,
	                   const XMLElement* primary, const XMLElement* secondary);
	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void createExpandedSlot(int ps);
	[[nodiscard]] int getAnyFreePrimarySlot();
	[[nodiscard]] int getSpecificFreePrimarySlot(unsigned slot);
	void addDevice(std::unique_ptr<MSXDevice> device);
	void setName(std::string_view proposedName);
	void setSlot(std::string_view slotname);

private:
	MSXMotherBoard& motherBoard;
	std::string hwName;
	Type type;
	std::string userName;
	XMLDocument config;
	FileContext context;

	bool externalSlots[4][4];
	bool externalPrimSlots[4];
	bool expandedSlots[4];
	bool allocatedPrimarySlots[4];

	std::vector<std::unique_ptr<MSXDevice>> devices;

	std::string name;

	friend struct SerializeConstructorArgs<HardwareConfig>;
};
SERIALIZE_CLASS_VERSION(HardwareConfig, 6);

template<> struct SerializeConstructorArgs<HardwareConfig>
{
	using type = std::tuple<std::string>;

	template<typename Archive>
	void save(Archive& ar, const HardwareConfig& config)
	{
		ar.serialize("hwname", config.hwName);
	}

	template<typename Archive>
	[[nodiscard]] type load(Archive& ar, unsigned /*version*/)
	{
		std::string name;
		ar.serialize("hwname", name);
		return std::tuple(name);
	}
};

} // namespace openmsx

#endif
