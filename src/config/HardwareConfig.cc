#include "HardwareConfig.hh"

#include "CartridgeSlotManager.hh"
#include "CommandController.hh"
#include "DeviceConfig.hh"
#include "DeviceFactory.hh"
#include "FileOperations.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "TclArgParser.hh"
#include "XMLException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"

#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <version> // for _LIBCPP_VERSION

namespace openmsx {

std::unique_ptr<HardwareConfig> HardwareConfig::createMachineConfig(
	MSXMotherBoard& motherBoard, std::string machineName)
{
	auto result = std::make_unique<HardwareConfig>(
		motherBoard, std::move(machineName));
	result->type = HardwareConfig::Type::MACHINE;
	result->load("machines");
	return result;
}

std::unique_ptr<HardwareConfig> HardwareConfig::createExtensionConfig(
	MSXMotherBoard& motherBoard, std::string extensionName, std::string_view slotName)
{
	auto result = std::make_unique<HardwareConfig>(
		motherBoard, std::move(extensionName));
	result->load("extensions");
	result->setName(result->hwName);
	result->type = HardwareConfig::Type::EXTENSION;
	result->setSlot(slotName);
	return result;
}

std::unique_ptr<HardwareConfig> HardwareConfig::createRomConfig(
	MSXMotherBoard& motherBoard, std::string_view romFile,
	std::string_view slotName, std::span<const TclObject> options)
{
	auto result = std::make_unique<HardwareConfig>(motherBoard, "rom");
	result->setName(romFile);
	result->type = HardwareConfig::Type::ROM;

	std::vector<std::string_view> ipsFiles;
	std::string mapper;
	std::array info = {
		valueArg("-ips", ipsFiles),
		valueArg("-romtype", mapper),
	};
	auto& interp = motherBoard.getCommandController().getInterpreter();
	if (auto args = parseTclArgs(interp, options, info);
	    !args.empty()) {
		throw MSXException("Invalid option \"", args.front().getString(), '\"');
	}

	const auto& sramfile = FileOperations::getFilename(romFile);
	auto context = userFileContext(tmpStrCat("roms/", sramfile));
	for (const auto& ips : ipsFiles) {
		if (!FileOperations::isRegularFile(context.resolve(ips))) {
			throw MSXException("Invalid IPS file: ", ips);
		}
	}

	std::string resolvedFilename = FileOperations::getAbsolutePath(
		context.resolve(romFile));
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
	const char* slotName2 = doc.allocateString(slotName);
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
	auto* mapperType = sound->setNextSibling(doc.allocateElement("mappertype"));
	mapperType->setData(mapper.empty() ? "auto" : doc.allocateString(mapper));
	auto* sramName = mapperType->setNextSibling(doc.allocateElement("sramname"));
	sramName->setData(doc.allocateString(tmpStrCat(sramfile, ".SRAM")));
	auto* rom = sramName->setNextSibling(doc.allocateElement("rom"));
	auto* rfName = rom->setFirstChild(doc.allocateElement("resolvedFilename"));
	rfName->setData(doc.allocateString(resolvedFilename));
	auto* fName = rfName->setNextSibling(doc.allocateElement("filename"));
	fName->setData(doc.allocateString(romFile));
	if (!ipsFiles.empty()) {
		auto* patches = fName->setNextSibling(doc.allocateElement("patches"));
		doc.generateList(*patches, "ips", ipsFiles, [&](XMLElement* n, auto& s) {
			n->setData(doc.allocateString(s));
		});
	}

	result->setConfig(extension);
	result->setFileContext(std::move(context));
	return result;
}

std::string_view HardwareConfig::getRomFilename() const
{
	// often this will give the same result as 'getName()', except when the same ROM file is used twice.
	assert(type == Type::ROM && "should only be used on ROM extensions");
	const auto* d = getConfig().findChild("devices"); assert(d);
	const auto* p = d->findChild("primary"); assert(p);
	const auto* s = p->findChild("secondary"); assert(s);
	const auto* R = s->findChild("ROM"); assert(R);
	const auto* r = R->findChild("rom"); assert(r);
	const auto* f = r->findChild("filename"); assert(f);
	return f->getData();
}

HardwareConfig::HardwareConfig(MSXMotherBoard& motherBoard_, std::string hwName_)
	: motherBoard(motherBoard_)
	, hwName(std::move(hwName_))
{
	for (auto& sub : externalSlots) {
		std::ranges::fill(sub, false);
	}
	std::ranges::fill(externalPrimSlots, false);
	std::ranges::fill(expandedSlots, false);
	std::ranges::fill(allocatedPrimarySlots, false);
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
		assert(false);
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
#ifdef _LIBCPP_VERSION
		// Workaround clang-13/libc++ bug
		// Don't generally use this workaround, because '*rit.base()'
		// triggers an error in a debug-STL build.
		std::span alreadyRemoved(std::to_address(rit.base()), et - rit.base());
#else
		std::span alreadyRemoved{rit.base(), et};
#endif
		(*rit)->testRemove(alreadyRemoved);
	}

	const auto& slotManager = motherBoard.getSlotManager();
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

XMLElement& HardwareConfig::getDevicesElem()
{
	return getConfig().getChild("devices");
}

static void loadHelper(XMLDocument& doc, const std::string& filename)
{
	try {
		doc.load(filename, "msxconfig2.dtd");
	} catch (XMLException& e) {
		throw MSXException(
			"Loading of hardware configuration failed: ",
			e.getMessage());
	}
}

static std::string getFilename(std::string_view type, std::string_view name)
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
	std::string filename = getFilename(type_, hwName);
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
	for (auto* psElem : getDevicesElem().getChildren("primary")) {
		auto& primSlot = psElem->getAttribute("slot");
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
				primSlot.setValue(config.allocateString(strCat(ps)));
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

uint8_t HardwareConfig::parseSlotMap() const
{
	uint8_t initialPrimarySlots = 0;
	if (const auto* slotMap = getConfig().findChild("slotmap")) {
		for (const auto* child : slotMap->getChildren("map")) {
			int page = child->getAttributeValueAsInt("page", -1);
			if (page < 0 || page > 3) {
				throw MSXException("Invalid or missing page in slotmap entry");
			}
			int slot = child->getAttributeValueAsInt("slot", -1);
			if (slot < 0 || slot > 3) {
				throw MSXException("Invalid or missing slot in slotmap entry");
			}
			unsigned offset = page * 2;
			initialPrimarySlots &= uint8_t(~(3 << offset));
			initialPrimarySlots |= uint8_t(slot << offset);
		}
	}
	return initialPrimarySlots;
}

void HardwareConfig::createDevices()
{
	createDevices(getDevicesElem(), nullptr, nullptr);
}

void HardwareConfig::createDevices(XMLElement& elem,
	XMLElement* primary, XMLElement* secondary)
{
	for (auto& c : elem.getChildren()) {
		const auto& childName = c.getName();
		if (childName == "primary") {
			createDevices(c, &c, secondary);
		} else if (childName == "secondary") {
			createDevices(c, primary, &c);
		} else {
			DeviceConfig config2(*this, c, primary, secondary);
			auto device = DeviceFactory::create(config2);
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

void HardwareConfig::setSlot(std::string_view slotName)
{
	for (auto* psElem : getDevicesElem().getChildren("primary")) {
		auto& primSlot = psElem->getAttribute("slot");
		if (primSlot.getValue() == "any") {
			primSlot.setValue(config.allocateString(slotName));
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
template<typename Archive>
void HardwareConfig::serialize(Archive& ar, unsigned version)
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
	if constexpr (Archive::IS_LOADER) {
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
		assert(Archive::IS_LOADER);
		type = name == "rom" ? HardwareConfig::Type::ROM : HardwareConfig::Type::EXTENSION;
	}
}
INSTANTIATE_SERIALIZE_METHODS(HardwareConfig);

} // namespace openmsx
