// $Id$

#include "HardwareConfig.hh"
#include "XMLLoader.hh"
#include "XMLException.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "LocalFileReference.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXMotherBoard.hh"
#include "CartridgeSlotManager.hh"
#include "MSXCPUInterface.hh"
#include "DeviceFactory.hh"
#include "CliComm.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "StringOp.hh"
#include "memory.hh"
#include "unreachable.hh"
#include <cassert>

#include <iostream>

using std::string;
using std::vector;
using std::unique_ptr;
using std::move;

namespace openmsx {

unique_ptr<HardwareConfig> HardwareConfig::createMachineConfig(
	MSXMotherBoard& motherBoard, const string& machineName)
{
	auto result = make_unique<HardwareConfig>(motherBoard, machineName);
	result->load("machines");
	return result;
}

unique_ptr<HardwareConfig> HardwareConfig::createExtensionConfig(
	MSXMotherBoard& motherBoard, const string& extensionName)
{
	auto result = make_unique<HardwareConfig>(motherBoard, extensionName);
	result->load("extensions");
	result->setName(extensionName);
	return result;
}

unique_ptr<HardwareConfig> HardwareConfig::createRomConfig(
	MSXMotherBoard& motherBoard, const string& romfile,
	const string& slotname, const vector<string>& options)
{
	auto result = make_unique<HardwareConfig>(motherBoard, "rom");
	string_ref sramfile = FileOperations::getFilename(romfile);
	auto context = make_unique<UserFileContext>("roms/" + sramfile);

	vector<string> ipsfiles;
	string mapper;

	bool romTypeOptionFound = false;

	// parse options
	for (auto it = options.begin(); it != options.end(); ++it) {
		const string& option = *it++;
		if (it == options.end()) {
			throw MSXException("Missing argument for option \"" +
			                   option + '\"');
		}
		if (option == "-ips") {
			if (!FileOperations::isRegularFile(context->resolve(*it))) {
				throw MSXException("Invalid IPS file: " + *it);
			}
			ipsfiles.push_back(*it);
		} else if (option == "-romtype") {
			if (!romTypeOptionFound) {
				mapper = *it;
				romTypeOptionFound = true;
			} else {
				throw MSXException("Only one -romtype option is allowed");
			}
		} else {
			throw MSXException("Invalid option \"" + option + '\"');
		}
	}

	string resolvedFilename = FileOperations::getAbsolutePath(
		context->resolve(romfile));
	if (!FileOperations::isRegularFile(resolvedFilename)) {
		throw MSXException("Invalid ROM file: " + resolvedFilename);
	}

	auto extension = make_unique<XMLElement>("extension");
	auto devices   = make_unique<XMLElement>("devices");
	auto primary   = make_unique<XMLElement>("primary");
	primary->addAttribute("slot", slotname);
	auto secondary = make_unique<XMLElement>("secondary");
	secondary->addAttribute("slot", slotname);
	auto device = make_unique<XMLElement>("ROM");
	device->addAttribute("id", "MSXRom");
	auto mem = make_unique<XMLElement>("mem");
	mem->addAttribute("base", "0x0000");
	mem->addAttribute("size", "0x10000");
	device->addChild(move(mem));
	auto rom = make_unique<XMLElement>("rom");
	rom->addChild(make_unique<XMLElement>(
		"resolvedFilename", resolvedFilename));
	rom->addChild(make_unique<XMLElement>("filename", romfile));
	if (!ipsfiles.empty()) {
		auto patches = make_unique<XMLElement>("patches");
		for (auto it = ipsfiles.begin(); it != ipsfiles.end(); ++it) {
			patches->addChild(make_unique<XMLElement>("ips", *it));
		}
		rom->addChild(move(patches));
	}
	device->addChild(move(rom));
	auto sound = make_unique<XMLElement>("sound");
	sound->addChild(make_unique<XMLElement>("volume", "9000"));
	device->addChild(move(sound));
	device->addChild(make_unique<XMLElement>(
		"mappertype", mapper.empty() ? "auto" : mapper));
	device->addChild(make_unique<XMLElement>(
		"sramname", sramfile + ".SRAM"));

	secondary->addChild(move(device));
	primary->addChild(move(secondary));
	devices->addChild(move(primary));
	extension->addChild(move(devices));

	result->setConfig(move(extension));
	result->setName(romfile);
	result->setFileContext(move(context));

	return result;
}

HardwareConfig::HardwareConfig(MSXMotherBoard& motherBoard_, const string& hwName_)
	: motherBoard(motherBoard_)
	, hwName(hwName_)
{
	for (int ps = 0; ps < 4; ++ps) {
		for (int ss = 0; ss < 4; ++ss) {
			externalSlots[ps][ss] = false;
		}
		externalPrimSlots[ps] = false;
		expandedSlots[ps] = false;
		allocatedPrimarySlots[ps] = false;
	}
	userName = motherBoard.getUserName(hwName);
}

HardwareConfig::~HardwareConfig()
{
	motherBoard.freeUserName(hwName, userName);
#ifndef NDEBUG
	try {
		testRemove();
	} catch (MSXException& e) {
		std::cerr << e.getMessage() << std::endl;
		UNREACHABLE;
	}
#endif
	while (!devices.empty()) {
		motherBoard.removeDevice(*devices.back());
		devices.pop_back();
	}
	CartridgeSlotManager& slotManager = motherBoard.getSlotManager();
	for (int ps = 0; ps < 4; ++ps) {
		for (int ss = 0; ss < 4; ++ss) {
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
	std::vector<MSXDevice*> alreadyRemoved;
	for (auto it = devices.rbegin(); it != devices.rend(); ++it) {
		(*it)->testRemove(alreadyRemoved);
		alreadyRemoved.push_back(it->get());
	}
	CartridgeSlotManager& slotManager = motherBoard.getSlotManager();
	for (int ps = 0; ps < 4; ++ps) {
		for (int ss = 0; ss < 4; ++ss) {
			if (externalSlots[ps][ss]) {
				slotManager.testRemoveExternalSlot(ps, ss, *this);
			}
		}
		if (externalPrimSlots[ps]) {
			slotManager.testRemoveExternalSlot(ps, *this);
		}
		if (expandedSlots[ps]) {
			motherBoard.getCPUInterface().testUnsetExpanded(
				ps, alreadyRemoved);
		}
	}
}

const FileContext& HardwareConfig::getFileContext() const
{
	return *context;
}
void HardwareConfig::setFileContext(unique_ptr<FileContext> context_)
{
	context = move(context_);
}

const XMLElement& HardwareConfig::getConfig() const
{
	return *config;
}

void HardwareConfig::setConfig(unique_ptr<XMLElement> newConfig)
{
	assert(!config.get());
	config = move(newConfig);
}

const XMLElement& HardwareConfig::getDevices() const
{
	return getConfig().getChild("devices");
}

unique_ptr<XMLElement> HardwareConfig::loadConfig(const string& filename)
{
	try {
		LocalFileReference fileRef(filename);
		return XMLLoader::load(fileRef.getFilename(), "msxconfig2.dtd");
	} catch (XMLException& e) {
		throw MSXException(
			"Loading of hardware configuration failed: " +
			e.getMessage());
	}
}

void HardwareConfig::load(string_ref path)
{
	string filename = SystemFileContext().resolve(
		FileOperations::join(path, hwName, "hardwareconfig.xml"));
	setConfig(loadConfig(filename));

	assert(!userName.empty());
	string_ref baseName = FileOperations::getBaseName(filename);
	setFileContext(make_unique<ConfigFileContext>(
		baseName, hwName, userName));
}

void HardwareConfig::parseSlots()
{
	// TODO this code does parsing for both 'expanded' and 'external' slots
	//      once machine and extensions are parsed separately move parsing
	//      of 'expanded' to MSXCPUInterface
	//
	auto primarySlots = getDevices().getChildren("primary");
	for (auto it = primarySlots.begin(); it != primarySlots.end(); ++it) {
		const string& primSlot = (*it)->getAttribute("slot");
		int ps = CartridgeSlotManager::getSlotNum(primSlot);
		if ((*it)->getAttributeAsBool("external", false)) {
			if (ps < 0) {
				throw MSXException(
				    "Cannot mark unspecified primary slot '" +
				    primSlot + "' as external");
			}
			createExternalSlot(ps);
			continue;
		}
		auto secondarySlots = (*it)->getChildren("secondary");
		for (auto it2 = secondarySlots.begin();
		     it2 != secondarySlots.end(); ++it2) {
			const string& secSlot = (*it2)->getAttribute("slot");
			int ss = CartridgeSlotManager::getSlotNum(secSlot);
			if (ss < 0) {
				if ((ss >= -128) && (0 <= ps) && (ps < 4) &&
				    motherBoard.getCPUInterface().isExpanded(ps)) {
					ss += 128;
				} else {
					continue;
				}
			}
			if (ps < 0) {
				ps = getFreePrimarySlot();
				XMLElement* mutableElem = const_cast<XMLElement*>(*it);
				mutableElem->setAttribute("slot", StringOp::toString(ps));
			}
			createExpandedSlot(ps);
			if ((*it2)->getAttributeAsBool("external", false)) {
				createExternalSlot(ps, ss);
			}
		}
	}
}

void HardwareConfig::createDevices()
{
	createDevices(getDevices(), nullptr, nullptr);
}

void HardwareConfig::createDevices(const XMLElement& elem,
	const XMLElement* primary, const XMLElement* secondary)
{
	const XMLElement::Children& children = elem.getChildren();
	for (auto it = children.begin(); it != children.end(); ++it) {
		const XMLElement& sub = **it;
		const string& name = sub.getName();
		if (name == "primary") {
			createDevices(sub, &sub, secondary);
		} else if (name == "secondary") {
			createDevices(sub, primary, &sub);
		} else {
			auto device = DeviceFactory::create(
				DeviceConfig(*this, sub, primary, secondary));
			if (device.get()) {
				addDevice(std::move(device));
			} else {
				motherBoard.getMSXCliComm().printWarning(
					"Deprecated device: \"" +
					name + "\", please upgrade your "
					"hardware descriptions.");
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

int HardwareConfig::getFreePrimarySlot()
{
	int ps;
	motherBoard.getSlotManager().allocatePrimarySlot(ps, *this);
	assert(!allocatedPrimarySlots[ps]);
	allocatedPrimarySlots[ps] = true;
	return ps;
}

void HardwareConfig::addDevice(std::unique_ptr<MSXDevice> device)
{
	motherBoard.addDevice(*device);
	devices.push_back(std::move(device));
}

const string& HardwareConfig::getName() const
{
	return name;
}

void HardwareConfig::setName(const string& proposedName)
{
	if (!motherBoard.findExtension(proposedName)) {
		name = proposedName;
	} else {
		unsigned n = 0;
		do {
			name = StringOp::Builder() << proposedName << " (" << ++n << ')';
		} while (motherBoard.findExtension(name));
	}
}


// version 1: initial version
// version 2: moved FileContext here (was part of config)
template<typename Archive>
void HardwareConfig::serialize(Archive& ar, unsigned version)
{
	// filled-in by constructor:
	//   motherBoard, hwName, userName
	// filled-in by parseSlots()
	//   externalSlots, externalPrimSlots, expandedSlots, allocatedPrimarySlots

	if (ar.versionBelow(version, 2)) {
		XMLElement::getLastSerializedFileContext(); // clear any previous value
	}
	ar.serialize("config", config); // fills in getLastSerializedFileContext()
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("context", context);
	} else {
		context = XMLElement::getLastSerializedFileContext();
		assert(context.get());
	}
	if (ar.isLoader()) {
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
	for (auto it = devices.begin(); it != devices.end(); ++it) {
		ar.serializePolymorphic("device", **it);
	}
	ar.serialize("name", name);
}
INSTANTIATE_SERIALIZE_METHODS(HardwareConfig);

} // namespace openmsx
