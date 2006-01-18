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
#include <cassert>

using std::string;

namespace openmsx {

HardwareConfig::HardwareConfig(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, reservedSlot(-1)
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
	CartridgeSlotManager& slotManager = motherBoard.getSlotManager();
	if (reservedSlot != -1) {
		slotManager.unreserveSlot(reservedSlot);
	}
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
		throw FatalError(
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
				throw FatalError("Cannot mark unspecified primary slot '" +
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

void HardwareConfig::reserveSlot(int slot)
{
	assert(reservedSlot == -1);
	motherBoard.getSlotManager().reserveSlot(slot);
	reservedSlot = slot;
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
		expandedSlots[ps] = true;
		motherBoard.getCPUInterface().setExpanded(ps);
		
	}
}

int HardwareConfig::getFreePrimarySlot()
{
	int ps;
	int slot = motherBoard.getSlotManager().getFreePrimarySlot(ps);
	assert(allocatedPrimarySlots[ps] == -1);
	allocatedPrimarySlots[ps] = slot;
	return ps;
}

} // namespace openmsx
