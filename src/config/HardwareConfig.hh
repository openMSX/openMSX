#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "FileContext.hh"
#include "XMLElement.hh"
#include "openmsx.hh"
#include "serialize_constr.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class MSXDevice;
class TclObject;

class HardwareConfig
{
public:
	enum class Type : uint8_t {
		MACHINE,
		EXTENSION,
		ROM
	};

	static void loadConfig(XMLDocument& doc, std::string_view type, std::string_view name);

	[[nodiscard]] static std::unique_ptr<HardwareConfig> createMachineConfig(
		MSXMotherBoard& motherBoard, std::string machineName);
	[[nodiscard]] static std::unique_ptr<HardwareConfig> createExtensionConfig(
		MSXMotherBoard& motherBoard, std::string extensionName,
		std::string_view slotName);
	[[nodiscard]] static std::unique_ptr<HardwareConfig> createRomConfig(
		MSXMotherBoard& motherBoard, std::string_view romFile,
		std::string_view slotName, std::span<const TclObject> options);

	HardwareConfig(MSXMotherBoard& motherBoard, std::string hwName);
	HardwareConfig(const HardwareConfig&) = delete;
	HardwareConfig(HardwareConfig&&) = delete;
	HardwareConfig& operator=(const HardwareConfig&) = delete;
	HardwareConfig& operator=(HardwareConfig&&) = delete;
	~HardwareConfig();

	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

	[[nodiscard]] const FileContext& getFileContext() const { return context; }
	void setFileContext(FileContext&& ctxt) { context = std::move(ctxt); }

	[[nodiscard]] XMLDocument& getXMLDocument() { return config; }
	[[nodiscard]] const XMLElement& getConfig() const { return *config.getRoot(); }
	[[nodiscard]] XMLElement& getConfig() { return *config.getRoot(); }
	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] const std::string& getConfigName() const { return hwName; }
	[[nodiscard]] std::string_view getRomFilename() const;
	[[nodiscard]] const XMLElement& getDevicesElem() const;
	[[nodiscard]] XMLElement& getDevicesElem();
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

	void createDevices(XMLElement& elem, XMLElement* primary, XMLElement* secondary);
	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void createExpandedSlot(int ps);
	[[nodiscard]] int getAnyFreePrimarySlot();
	[[nodiscard]] int getSpecificFreePrimarySlot(unsigned slot);
	void addDevice(std::unique_ptr<MSXDevice> device);
	void setName(std::string_view proposedName);
	void setSlot(std::string_view slotName);

private:
	MSXMotherBoard& motherBoard;
	std::string hwName;
	Type type;
	std::string userName;
	XMLDocument config{8192}; // tweak: initial allocator buffer size
	FileContext context;

	std::array<std::array<bool, 4>, 4> externalSlots;
	std::array<bool, 4> externalPrimSlots;
	std::array<bool, 4> expandedSlots;
	std::array<bool, 4> allocatedPrimarySlots;

	std::vector<std::unique_ptr<MSXDevice>> devices;

	std::string name;

	friend struct SerializeConstructorArgs<HardwareConfig>;
};
SERIALIZE_CLASS_VERSION(HardwareConfig, 6);

template<> struct SerializeConstructorArgs<HardwareConfig>
{
	using type = std::tuple<std::string>;

	template<typename Archive>
	void save(Archive& ar, const HardwareConfig& config) const
	{
		ar.serialize("hwname", config.hwName);
	}

	template<typename Archive>
	[[nodiscard]] type load(Archive& ar, unsigned /*version*/) const
	{
		std::string name;
		ar.serialize("hwname", name);
		return {name};
	}
};

} // namespace openmsx

#endif
