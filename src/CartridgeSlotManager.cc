// $Id$

#include <cassert>
#include "openmsx.hh"
#include "StringOp.hh"
#include "CartridgeSlotManager.hh"
#include "HardwareConfig.hh"

namespace openmsx {

const int RESERVED = 16;
const int EXISTS   = 32;


CartridgeSlotManager::CartridgeSlotManager()
	: hardwareConfig(HardwareConfig::instance())
{
	for (int slot = 0; slot < 16; ++slot) {
		slots[slot] = 0;
	}
	slotCounter = 0;
}

CartridgeSlotManager::~CartridgeSlotManager()
{
}

CartridgeSlotManager& CartridgeSlotManager::instance()
{
	static CartridgeSlotManager oneInstance;
	return oneInstance;
}


void CartridgeSlotManager::reserveSlot(int slot)
{
	assert((0 <= slot) && (slot < 16));
	slots[slot] |= RESERVED;
}

void CartridgeSlotManager::readConfig()
{
	XMLElement::Children primarySlots;
	hardwareConfig.findChild("devices")->getChildren("primary", primarySlots);
	for (XMLElement::Children::const_iterator it = primarySlots.begin();
	     it != primarySlots.end(); ++it) {
		const string& primSlot = (*it)->getAttribute("slot");
		if (primSlot == "any") {
			continue; // TODO
		}
		unsigned primNum = StringOp::stringToInt(primSlot);
		if (primNum >= 4) {
			throw FatalError("Invalid slot specification");
		}
		if ((*it)->getAttributeAsBool("external", false)) {
			createExternal(primNum, 0);
			continue;
		}
		XMLElement::Children secondarySlots;
		(*it)->getChildren("secondary", secondarySlots);
		for (XMLElement::Children::const_iterator it2 = secondarySlots.begin();
		     it2 != secondarySlots.end(); ++it2) {
			if ((*it2)->getAttributeAsBool("external", false)) {
				const string& secSlot = (*it2)->getAttribute("slot");
				if (secSlot == "any") {
					continue;
				}
				unsigned secNum = StringOp::stringToInt(secSlot);
				if (secNum >= 4) {
					throw FatalError("Invalid slot specification");
				}
				createExternal(primNum, secNum);
			}
		}
	}
}

void CartridgeSlotManager::createExternal(unsigned ps, unsigned ss)
{
	PRT_DEBUG("External slot : " << ps << "-" << ss);
	slots[slotCounter] |= (ss << 2) | ps | EXISTS;
	++slotCounter;
}

void CartridgeSlotManager::getSlot(int slot, int &ps, int &ss)
{
	assert((0 <= slot) && (slot < 16));
	assert(slots[slot] & RESERVED);

	if (slots[slot] & EXISTS) {
		ps = slots[slot] & 3;
		ss = (slots[slot] >> 2) & 3;
	} else {
		throw FatalError(string("Slot") + (char)('a' + slot) + " not defined");
	}
}

void CartridgeSlotManager::getSlot(int &ps, int &ss)
{
	for (int slot = 0; slot < 16; slot++) {
		if ((slots[slot] & EXISTS) && !(slots[slot] & RESERVED)) {
			slots[slot] |= RESERVED;
			ps = slots[slot] & 3;
			ss = (slots[slot] >> 2) & 3;
			return;
		}
	}
	throw FatalError("Not enough free cartridge slots");
}

} // namespace openmsx
