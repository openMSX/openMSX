// $Id$

#include "HardwareConfig.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXMotherBoard.hh"
#include "CartridgeSlotManager.hh"
#include "MSXCPUInterface.hh"
#include "DeviceFactory.hh"
#include "Scheduler.hh"
#include "CliComm.hh"
#include <cassert>

#include <iostream>

using std::string;

namespace openmsx {

HardwareConfig::HardwareConfig(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	for (int ps = 0; ps < 4; ++ps) {
		for (int ss = 0; ss < 4; ++ss) {
			externalSlots[ps][ss] = false;
		}
		externalPrimSlots[ps] = false;
		expandedSlots[ps] = false;
		allocatedPrimarySlots[ps] = -1;
	}
}

HardwareConfig::~HardwareConfig()
{
#ifndef NDEBUG
	try {
		testRemove();
	} catch (MSXException& e) {
		std::cerr << e.getMessage() << std::endl;
		assert(false);
	}
#endif
	for (Devices::reverse_iterator it = devices.rbegin();
	     it != devices.rend(); ++it) {
		motherBoard.removeDevice(**it);
		delete *it;
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
		if (allocatedPrimarySlots[ps] != -1) {
			slotManager.freeSlot(allocatedPrimarySlots[ps]);
		}
	}
}

void HardwareConfig::testRemove() const
{
	Devices alreadyRemoved;
	for (Devices::const_reverse_iterator it = devices.rbegin();
	     it != devices.rend(); ++it) {
		(*it)->testRemove(alreadyRemoved);
		alreadyRemoved.push_back(*it);
	}
	CartridgeSlotManager& slotManager = motherBoard.getSlotManager();
	for (int ps = 0; ps < 4; ++ps) {
		for (int ss = 0; ss < 4; ++ss) {
			if (externalSlots[ps][ss]) {
				slotManager.testRemoveExternalSlot(ps, ss);
			}
		}
		if (externalPrimSlots[ps]) {
			slotManager.testRemoveExternalSlot(ps);
		}
		if (expandedSlots[ps]) {
			motherBoard.getCPUInterface().testUnsetExpanded(
				ps, alreadyRemoved);
		}
	}
}

const XMLElement& HardwareConfig::getConfig() const
{
	return *config;
}

void HardwareConfig::setConfig(std::auto_ptr<XMLElement> newConfig)
{
	assert(!config.get());
	config = newConfig;
}

void HardwareConfig::load(const string& path, const string& hwName)
{
	SystemFileContext context;
	string filename;
	try {
		filename = context.resolve(path + '/' + hwName + ".xml");
	} catch (FileException& e) {
		filename = context.resolve(
			path + '/' + hwName + "/hardwareconfig.xml");
	}
	try {
		File file(filename);
		config = XMLLoader::loadXML(file.getLocalName(),
		                            "msxconfig2.dtd");

		// get url
		string url(file.getURL());
		string::size_type pos = url.find_last_of('/');
		assert(pos != string::npos);	// protocol must contain a '/'
		url = url.substr(0, pos);

		// TODO get user name
		string userName;
		config->setFileContext(std::auto_ptr<FileContext>(
			new ConfigFileContext(url + '/', hwName, userName)));
	} catch (XMLException& e) {
		throw MSXException(
			"Loading of hardware configuration failed: " +
			e.getMessage());
	}
}

void HardwareConfig::parseSlots()
{
	// TODO this code does parsing for both 'expanded' and 'external' slots
	//      once machine and extensions are parsed separately move parsing
	//      of 'expanded' to MSXCPUInterface
	//
	XMLElement::Children primarySlots;
	getDevices().getChildren("primary", primarySlots);
	for (XMLElement::Children::const_iterator it = primarySlots.begin();
	     it != primarySlots.end(); ++it) {
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
		XMLElement::Children secondarySlots;
		(*it)->getChildren("secondary", secondarySlots);
		for (XMLElement::Children::const_iterator it2 = secondarySlots.begin();
		     it2 != secondarySlots.end(); ++it2) {
			const string& secSlot = (*it2)->getAttribute("slot");
			int ss = CartridgeSlotManager::getSlotNum(secSlot);
			if (ss < 0) {
				continue;
			}
			if (ps < 0) {
				ps = getFreePrimarySlot();
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
	createDevices(getDevices());
}

void HardwareConfig::createDevices(const XMLElement& elem)
{
	const XMLElement::Children& children = elem.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		const XMLElement& sub = **it;
		const string& name = sub.getName();
		if ((name == "primary") || (name == "secondary")) {
			createDevices(sub);
		} else {
			std::auto_ptr<MSXDevice> device(DeviceFactory::create(
				motherBoard, *this, sub,
				motherBoard.getScheduler().getCurrentTime()));
			if (device.get()) {
				addDevice(device.release());
			} else {
				motherBoard.getCliComm().printWarning(
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
	int slot = motherBoard.getSlotManager().getFreePrimarySlot(ps, *this);
	assert(allocatedPrimarySlots[ps] == -1);
	allocatedPrimarySlots[ps] = slot;
	return ps;
}

void HardwareConfig::addDevice(MSXDevice* device)
{
	motherBoard.addDevice(*device);
	devices.push_back(device);
}

MSXMotherBoard& HardwareConfig::getMotherBoard()
{
	return motherBoard;
}

} // namespace openmsx
