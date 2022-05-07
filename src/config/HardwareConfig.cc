#include "HardwareConfig.hh"
#include "XMLException.hh"
#include "DeviceConfig.hh"
#include "FileOperations.hh"
#include "MSXMotherBoard.hh"
#include "CartridgeSlotManager.hh"
#include "MSXCPUInterface.hh"
#include "CommandController.hh"
#include "DeviceFactory.hh"
#include "TclArgParser.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <iostream>
#include <memory>

using std::string;

namespace openmsx {

std::unique_ptr<HardwareConfig> HardwareConfig::createMachineConfig(
	MSXMotherBoard& motherBoard, string machineName)
{
	auto result = std::make_unique<HardwareConfig>(
		motherBoard, std::move(machineName));
	result->type = HardwareConfig::Type::MACHINE;
	result->load("machines");
	return result;
}

std::unique_ptr<HardwareConfig> HardwareConfig::createExtensionConfig(
	MSXMotherBoard& motherBoard, string extensionName, std::string_view slotname)
{
	auto result = std::make_unique<HardwareConfig>(
		motherBoard, std::move(extensionName));
	result->load("extensions");
	result->setName(result->hwName);
	result->type = HardwareConfig::Type::EXTENSION;
	result->setSlot(slotname);
	return result;
}

std::unique_ptr<HardwareConfig> HardwareConfig::createRomConfig(
	MSXMotherBoard& motherBoard, string romfile,
	string slotname, std::span<const TclObject> options)
{
	auto result = std::make_unique<HardwareConfig>(motherBoard, "rom");
	result->setName(romfile);
	result->type = HardwareConfig::Type::ROM;

	std::vector<std::string_view> ipsfiles;
	string mapper;
	ArgsInfo info[] = {
		valueArg("-ips", ipsfiles),
		valueArg("-romtype", mapper),
	};
	auto& interp = motherBoard.getCommandController().getInterpreter();
	auto args = parseTclArgs(interp, options, info);
	if (!args.empty()) {
		throw MSXException("Invalid option \"", args.front().getString(), '\"');
	}

	const auto& sramfile = FileOperations::getFilename(romfile);
	auto context = userFileContext(tmpStrCat("roms/", sramfile));
	for (const auto& ips : ipsfiles) {
		if (!FileOperations::isRegularFile(context.resolve(ips))) {
			throw MSXException("Invalid IPS file: ", ips);
		}
	}

	string resolvedFilename = FileOperations::getAbsolutePath(
		context.resolve(romfile));
	if (!FileOperations::isRegularFile(resolvedFilename)) {
		throw MSXException("Invalid ROM file: ", resolvedFilename);
	}

	//<extension>
	//  <devices>
	//    <primary slot="...">
	//      <secondary slot="...">
	//        <ROM id="MSXRom">
	//          <mem base="0x0000" size="0x10000"/>
	//          <sound>
	//            <volume>9000</volume>
	//          </sound>
	//          <mappertype>...</mappertype>
	//          <sramname>...SRAM</sramname>
	//          <rom>
	//            <resolvedFilename>...</resolvedFilename>
	//            <filename>...</filename>
	//            <patches>
	//              <ips>...</ips>
	//              ...
	//            </patches>
	//          </rom>
	//        </ROM>
	//      </secondary>
	//    </primary>
	//  </devices>
	//</extension>
	XMLDocument& doc = result->config;
	auto* extension = doc.allocateElement("extension");
	auto* devices = extension->setFirstChild(doc.allocateElement("devices"));
	auto* primary = devices->setFirstChild(doc.allocateElement("primary"));
	const char* slotName2 = doc.allocateString(slotname);
	primary->setFirstAttribute(doc.allocateAttribute("slot", slotName2));
	auto* secondary = primary->setFirstChild(doc.allocateElement("secondary"));
	secondary->setFirstAttribute(doc.allocateAttribute("slot", slotName2));
	auto* device = secondary->setFirstChild(doc.allocateElement("ROM"));
	device->setFirstAttribute(doc.allocateAttribute("id", "MSXRom"));
	auto* mem = device->setFirstChild(doc.allocateElement("mem"));
	mem->setFirstAttribute(doc.allocateAttribute("base", "0x0000"))
	   ->setNextAttribute (doc.allocateAttribute("size", "0x10000"));
	auto* sound = mem->setNextSibling(doc.allocateElement("sound"));
	sound->setFirstChild(doc.allocateElement("volume", "9000"));
	auto* mappertype = sound->setNextSibling(doc.allocateElement("mappertype"));
	mappertype->setData(mapper.empty() ? "auto" : doc.allocateString(mapper));
	auto* sramname = mappertype->setNextSibling(doc.allocateElement("sramname"));
	sramname->setData(doc.allocateString(tmpStrCat(sramfile, ".SRAM")));
	auto* rom = sramname->setNextSibling(doc.allocateElement("rom"));
	auto* rfName = rom->setFirstChild(doc.allocateElement("resolvedFilename"));
	rfName->setData(doc.allocateString(resolvedFilename));
	auto* fName = rfName->setNextSibling(doc.allocateElement("filename"));
	fName->setData(doc.allocateString(romfile));
	if (!ipsfiles.empty()) {
		auto* patches = fName->setNextSibling(doc.allocateElement("patches"));
		doc.generateList(*patches, "ips", ipsfiles, [&](XMLElement* n, auto& s) {
			n->setData(doc.allocateString(s));
		});
	}

	result->setConfig(extension);
	result->setFileContext(std::move(context));
	return result;
}

HardwareConfig::HardwareConfig(MSXMotherBoard& motherBoard_, string hwName_)
	: motherBoard(motherBoard_)
	, hwName(std::move(hwName_))
	, config(8192) // tweak: initial allocator buffer size
{
	for (auto& sub : externalSlots) {
		ranges::fill(sub, false);
	}
	ranges::fill(externalPrimSlots, false);
	ranges::fill(expandedSlots, false);
	ranges::fill(allocatedPrimarySlots, false);
	userName = motherBoard.getUserName(hwName);
}

HardwareConfig::~HardwareConfig()
{
	motherBoard.freeUserName(hwName, userName);
#ifndef NDEBUG
	try {
		testRemove();
	} catch (MSXException& e) {
		std::cerr << e.getMessage() << '\n';
		UNREACHABLE;
	}
#endif
	while (!devices.empty()) {
		motherBoard.removeDevice(*devices.back());
		devices.pop_back();
	}
	auto& slotManager = motherBoard.getSlotManager();
	for (auto ps : xrange(4)) {
		for (auto ss : xrange(4)) {
			if (externalSlots[ps][ss]) {
				slotManager.removeExternalSlot(ps, ss);
			}
		}
		if (externalPrimSlots[ps]) {
			slotManager.removeExternalSlot(ps);
		}
		if (expandedSlots[ps]) {
			motherBoard.getCPUInterface().unsetExpanded(ps);
		}
		if (allocatedPrimarySlots[ps]) {
			slotManager.freePrimarySlot(ps, *this);
		}
	}
}

void HardwareConfig::testRemove() const
{
	auto et = devices.end();
	for (auto rit = devices.rbegin(), ret = devices.rend();
	     rit != ret; ++rit) {
		// Workaround clang-13/libc++ bug
		//std::span alreadyRemoved{rit.base(), et};
		std::span alreadyRemoved(&*rit.base(), et - rit.base());
		(*rit)->testRemove(alreadyRemoved);
	}

	auto& slotManager = motherBoard.getSlotManager();
	for (auto ps : xrange(4)) {
		for (auto ss : xrange(4)) {
			if (externalSlots[ps][ss]) {
				slotManager.testRemoveExternalSlot(ps, ss, *this);
			}
		}
		if (externalPrimSlots[ps]) {
			slotManager.testRemoveExternalSlot(ps, *this);
		}
		if (expandedSlots[ps]) {
			motherBoard.getCPUInterface().testUnsetExpanded(
				ps, devices);
		}
	}
}

const XMLElement& HardwareConfig::getDevicesElem() const
{
	return getConfig().getChild("devices");
}

static void loadHelper(XMLDocument& doc, std::string filename)
{
	try {
		doc.load(filename, "msxconfig2.dtd");
	} catch (XMLException& e) {
		throw MSXException(
			"Loading of hardware configuration failed: ",
			e.getMessage());
	}
}

static string getFilename(std::string_view type, std::string_view name)
{
	const auto& context = systemFileContext();
	try {
		// try <name>.xml
		return context.resolve(tmpStrCat(
			type, '/', name, ".xml"));
	} catch (MSXException& e) {
		// backwards-compatibility:
		//  also try <name>/hardwareconfig.xml
		try {
			return context.resolve(tmpStrCat(
				type, '/', name, "/hardwareconfig.xml"));
		} catch (MSXException&) {
			throw e; // signal first error
		}
	}
}

void HardwareConfig::loadConfig(XMLDocument& doc, std::string_view type, std::string_view name)
{
	loadHelper(doc, getFilename(type, name));
}

void HardwareConfig::load(std::string_view type_)
{
	string filename = getFilename(type_, hwName);
	loadHelper(config, filename);

	assert(!userName.empty());
	const auto& dirname = FileOperations::getDirName(filename);
	setFileContext(configFileContext(dirname, hwName, userName));
}

void HardwareConfig::parseSlots()
{
	// TODO this code does parsing for both 'expanded' and 'external' slots
	//      once machine and extensions are parsed separately move parsing
	//      of 'expanded' to MSXCPUInterface
	//
	for (const auto* psElem : getDevicesElem().getChildren("primary")) {
		const auto& primSlot = psElem->getAttribute("slot");
		int ps = CartridgeSlotManager::getSlotNum(primSlot.getValue());
		if (psElem->getAttributeValueAsBool("external", false)) {
			if (ps < 0) {
				throw MSXException(
					"Cannot mark unspecified primary slot '",
					primSlot.getValue(), "' as external");
			}
			if (psElem->hasChildren()) {
				throw MSXException(
					"Primary slot ", ps,
					" is marked as external, but that would only "
					"make sense if its <primary> tag would be "
					"empty.");
			}
			createExternalSlot(ps);
			continue;
		}
		for (const auto* ssElem : psElem->getChildren("secondary")) {
			auto secSlot = ssElem->getAttributeValue("slot");
			int ss = CartridgeSlotManager::getSlotNum(secSlot);
			if ((-16 <= ss) && (ss <= -1) && (ss != ps)) {
				throw MSXException(
					"Invalid secondary slot specification: \"",
					secSlot, "\".");
			}
			if (ss < 0) {
				if ((ss >= -128) && (0 <= ps) && (ps < 4) &&
				    motherBoard.getCPUInterface().isExpanded(ps)) {
					ss += 128;
				} else {
					continue;
				}
			}
			if (ps < 0) {
				if (ps == -256) {
					ps = getAnyFreePrimarySlot();
				} else {
					ps = getSpecificFreePrimarySlot(-ps - 1);
				}
				auto& mutablePrimSlot = const_cast<XMLAttribute&>(primSlot);
				mutablePrimSlot.setValue(config.allocateString(strCat(ps)));
			}
			createExpandedSlot(ps);
			if (ssElem->getAttributeValueAsBool("external", false)) {
				if (ssElem->hasChildren()) {
					throw MSXException(
						"Secondary slot ", ps, '-', ss,
						" is marked as external, but that would "
						"only make sense if its <secondary> tag "
						"would be empty.");
				}
				createExternalSlot(ps, ss);
			}
		}
	}
}

byte HardwareConfig::parseSlotMap() const
{
	byte initialPrimarySlots = 0;
	if (const auto* slotmap = getConfig().findChild("slotmap")) {
		for (const auto* child : slotmap->getChildren("map")) {
			int page = child->getAttributeValueAsInt("page", -1);
			if (page < 0 || page > 3) {
				throw MSXException("Invalid or missing page in slotmap entry");
			}
			int slot = child->getAttributeValueAsInt("slot", -1);
			if (slot < 0 || slot > 3) {
				throw MSXException("Invalid or missing slot in slotmap entry");
			}
			unsigned offset = page * 2;
			initialPrimarySlots &= ~(3 << offset);
			initialPrimarySlots |= slot << offset;
		}
	}
	return initialPrimarySlots;
}

void HardwareConfig::createDevices()
{
	createDevices(getDevicesElem(), nullptr, nullptr);
}

void HardwareConfig::createDevices(const XMLElement& elem,
	const XMLElement* primary, const XMLElement* secondary)
{
	for (const auto& c : elem.getChildren()) {
		const auto& childName = c.getName();
		if (childName == "primary") {
			createDevices(c, &c, secondary);
		} else if (childName == "secondary") {
			createDevices(c, primary, &c);
		} else {
			auto device = DeviceFactory::create(
				DeviceConfig(*this, c, primary, secondary));
			if (device) {
				addDevice(std::move(device));
			} else {
				// device is nullptr, so we are apparently ignoring it on purpose
			}
		}
	}
}

void HardwareConfig::createExternalSlot(int ps)
{
	motherBoard.getSlotManager().createExternalSlot(ps);
	assert(!externalPrimSlots[ps]);
	externalPrimSlots[ps] = true;
}

void HardwareConfig::createExternalSlot(int ps, int ss)
{
	motherBoard.getSlotManager().createExternalSlot(ps, ss);
	assert(!externalSlots[ps][ss]);
	externalSlots[ps][ss] = true;
}

void HardwareConfig::createExpandedSlot(int ps)
{
	if (!expandedSlots[ps]) {
		motherBoard.getCPUInterface().setExpanded(ps);
		expandedSlots[ps] = true;
	}
}

int HardwareConfig::getAnyFreePrimarySlot()
{
	int ps = motherBoard.getSlotManager().allocateAnyPrimarySlot(*this);
	assert(!allocatedPrimarySlots[ps]);
	allocatedPrimarySlots[ps] = true;
	return ps;
}

int HardwareConfig::getSpecificFreePrimarySlot(unsigned slot)
{
	int ps = motherBoard.getSlotManager().allocateSpecificPrimarySlot(slot, *this);
	assert(!allocatedPrimarySlots[ps]);
	allocatedPrimarySlots[ps] = true;
	return ps;
}

void HardwareConfig::addDevice(std::unique_ptr<MSXDevice> device)
{
	motherBoard.addDevice(*device);
	devices.push_back(std::move(device));
}

void HardwareConfig::setName(std::string_view proposedName)
{
	if (!motherBoard.findExtension(proposedName)) {
		name = proposedName;
	} else {
		unsigned n = 0;
		do {
			name = strCat(proposedName, " (", ++n, ')');
		} while (motherBoard.findExtension(name));
	}
}

void HardwareConfig::setSlot(std::string_view slotname)
{
	for (const auto* psElem : getDevicesElem().getChildren("primary")) {
		const auto& primSlot = psElem->getAttribute("slot");
		if (primSlot.getValue() == "any") {
			auto& mutablePrimSlot = const_cast<XMLAttribute&>(primSlot);
			mutablePrimSlot.setValue(config.allocateString(slotname));
		}
	}
}

static constexpr std::initializer_list<enum_string<HardwareConfig::Type>> configTypeInfo = {
	{ "MACHINE",   HardwareConfig::Type::MACHINE   },
	{ "EXTENSION", HardwareConfig::Type::EXTENSION },
	{ "ROM",       HardwareConfig::Type::ROM       },
};
SERIALIZE_ENUM(HardwareConfig::Type, configTypeInfo);

// version 1: initial version
// version 2: moved FileContext here (was part of config)
// version 3: hold 'config' by-value instead of by-pointer
// version 4: hold 'context' by-value instead of by-pointer
// version 5: added hwconfig type info
// version 6: switch from old to new XMLElement
template<Archive Ar>
void HardwareConfig::serialize(Ar& ar, unsigned version)
{
	// filled-in by constructor:
	//   motherBoard, hwName, userName
	// filled-in by parseSlots()
	//   externalSlots, externalPrimSlots, expandedSlots, allocatedPrimarySlots

	if (ar.versionAtLeast(version, 6)) {
		ar.serialize("config", config);
	} else {
		OldXMLElement elem;
		ar.serialize("config", elem); // fills in getLastSerializedFileContext()
		config.load(elem);
	}
	if (ar.versionAtLeast(version, 2)) {
		if (ar.versionAtLeast(version, 4)) {
			ar.serialize("context", context);
		} else {
			std::unique_ptr<FileContext> ctxt;
			ar.serialize("context", ctxt);
			if (ctxt) context = *ctxt;
		}
	} else {
		auto ctxt = OldXMLElement::getLastSerializedFileContext();
		assert(ctxt);
		context = *ctxt;
	}
	if constexpr (Ar::IS_LOADER) {
		if (!motherBoard.getMachineConfig()) {
			// must be done before parseSlots()
			motherBoard.setMachineConfig(this);
		} else {
			// already set because this is an extension
		}
		parseSlots();
		createDevices();
	}
	// only (polymorphically) initialize devices, they are already created
	for (auto& d : devices) {
		ar.serializePolymorphic("device", *d);
	}
	ar.serialize("name", name);
	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("type", type);
	} else {
		assert(Ar::IS_LOADER);
		type = name == "rom" ? HardwareConfig::Type::ROM : HardwareConfig::Type::EXTENSION;
	}
}
INSTANTIATE_SERIALIZE_METHODS(HardwareConfig);

} // namespace openmsx
