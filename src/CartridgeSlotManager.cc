// $Id$

#include <cassert>
#include "openmsx.hh"
#include "StringOp.hh"
#include "CartridgeSlotManager.hh"
#include "HardwareConfig.hh"
#include "MSXCPUInterface.hh"

namespace openmsx {

const int RESERVED = 16;
const int EXISTS   = 32;
const int PRIMARY  = 64;


CartridgeSlotManager::CartridgeSlotManager()
	: hardwareConfig(HardwareConfig::instance()),
	  cpuInterface(MSXCPUInterface::instance())
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

int CartridgeSlotManager::getSlotNum(const string& slot)
{
	if ((slot.size() == 1) && ('a' <= slot[0]) && (slot[0] <= 'p')) {
		return -(1 + slot[0] - 'a');
	} else if (slot == "any") {
		return -256;
	} else {
		int result = StringOp::stringToInt(slot);
		if ((result < 0) || (4 <= result)) {
			throw FatalError("Invalid slot specification: " + slot);
		}
		return result;
	}
}

void CartridgeSlotManager::readConfig()
{
	XMLElement::Children primarySlots;
	hardwareConfig.getChild("devices").getChildren("primary", primarySlots);
	for (XMLElement::Children::const_iterator it = primarySlots.begin();
	     it != primarySlots.end(); ++it) {
		const string& primSlot = (*it)->getAttribute("slot");
		int ps = getSlotNum(primSlot);
		if ((*it)->getAttributeAsBool("external", false)) {
			if (ps < 0) {
				throw FatalError("Cannot mark unspecified primary slot '" +
					primSlot + "' as external");
			}
			createExternal(ps);
			continue;
		}
		XMLElement::Children secondarySlots;
		(*it)->getChildren("secondary", secondarySlots);
		for (XMLElement::Children::const_iterator it2 = secondarySlots.begin();
		     it2 != secondarySlots.end(); ++it2) {
			const string& secSlot = (*it2)->getAttribute("slot");
			int ss = getSlotNum(secSlot);
			if (ss < 0) {
				continue;
			}
			if (ps < 0) {
				getSlot(ps);
			}
			cpuInterface.setExpanded(ps, true);
			if ((*it2)->getAttributeAsBool("external", false)) {
				createExternal(ps, ss);
			}
		}
	}
}

void CartridgeSlotManager::createExternal(unsigned ps)
{
	PRT_DEBUG("External slot : " << ps);
	slots[slotCounter] |= ps | EXISTS | PRIMARY;
	++slotCounter;
}

void CartridgeSlotManager::createExternal(unsigned ps, unsigned ss)
{
	PRT_DEBUG("External slot : " << ps << "-" << ss);
	slots[slotCounter] |= (ss << 2) | ps | EXISTS;
	++slotCounter;
}

void CartridgeSlotManager::getSlot(int slot, int& ps, int& ss)
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

void CartridgeSlotManager::getSlot(int& ps, int& ss)
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

void CartridgeSlotManager::getSlot(int& ps)
{
	for (int slot = 0; slot < 16; ++slot) {
		if ((slots[slot] & EXISTS) && !(slots[slot] & RESERVED) &&
		    (slots[slot] & PRIMARY)) {
			slots[slot] |= RESERVED;
			ps = slots[slot] & 3;
			return;
		}
	}
	throw FatalError("No free primary slot");
}

} // namespace openmsx
