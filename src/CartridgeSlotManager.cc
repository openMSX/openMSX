// $Id$

#include "CartridgeSlotManager.hh"
#include "MSXMotherBoard.hh"
#include "XMLElement.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include <cassert>

using std::string;

namespace openmsx {

CartridgeSlotManager::CartridgeSlotManager(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
}

CartridgeSlotManager::~CartridgeSlotManager()
{
	for (int slot = 0; slot < 16; ++slot) {
		assert(slots[slot].used == false);
	}
}

void CartridgeSlotManager::reserveSlot(int slot)
{
	assert((0 <= slot) && (slot < 16));
	slots[slot].reserved = true;
}

void CartridgeSlotManager::unreserveSlot(int slot)
{
	assert((0 <= slot) && (slot < 16));
	slots[slot].reserved = false;
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

void CartridgeSlotManager::readConfig(const XMLElement& config)
{
	// TODO this code does parsing for both 'expanded' and 'external' slots
	//      once machine and extensions are parsed separately move parsing
	//      of 'expanded' to MSXCPUInterface
	XMLElement::Children primarySlots;
	config.getChild("devices").getChildren("primary", primarySlots);
	for (XMLElement::Children::const_iterator it = primarySlots.begin();
	     it != primarySlots.end(); ++it) {
		const string& primSlot = (*it)->getAttribute("slot");
		int ps = getSlotNum(primSlot);
		if ((*it)->getAttributeAsBool("external", false)) {
			if (ps < 0) {
				throw FatalError("Cannot mark unspecified primary slot '" +
					primSlot + "' as external");
			}
			createExternal(ps, 0);
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
				ps = getFreePrimarySlot();
			}
			motherBoard.getCPUInterface().setExpanded(ps, true);
			if ((*it2)->getAttributeAsBool("external", false)) {
				createExternal(ps, ss);
			}
		}
	}
}

void CartridgeSlotManager::createExternal(int ps, int ss)
{
	for (int slot = 0; slot < 16; ++slot) {
		if (!slots[slot].exists) {
			slots[slot].ps = ps;
			slots[slot].ss = ss;
			slots[slot].exists = true;
			return;
		}
	}
	assert(false);
}

int CartridgeSlotManager::getReservedSlot(int slot, int& ps, int& ss)
{
	assert((0 <= slot) && (slot < 16));
	assert(slots[slot].reserved);

	if (!slots[slot].exists) {
		throw FatalError(string("Slot") + (char)('a' + slot) + " not defined");
	}
	ps = slots[slot].ps;
	ss = slots[slot].ss;
	slots[slot].used = true;
	return slot;
}

int CartridgeSlotManager::getAnyFreeSlot(int& ps, int& ss)
{
	for (int slot = 0; slot < 16; slot++) {
		if (slots[slot].exists &&
		    !slots[slot].reserved && !slots[slot].used) {
			slots[slot].used = true;
			ps = slots[slot].ps;
			ss = slots[slot].ss;
			return slot;
		}
	}
	throw FatalError("Not enough free cartridge slots");
}

void CartridgeSlotManager::freeSlot(int slot)
{
	assert(slots[slot].used);
	slots[slot].used = false;
}

int CartridgeSlotManager::getFreePrimarySlot()
{
	for (int slot = 0; slot < 16; ++slot) {
		int ps = slots[slot].ps;
		if (slots[slot].exists &&
		    !slots[slot].reserved && !slots[slot].used &&
		    !motherBoard.getCPUInterface().isExpanded(ps)) {
			assert(slots[slot].ss == 0);
			slots[slot].reserved = true;
			return ps;
		}
	}
	throw FatalError("No free primary slot");
}

bool CartridgeSlotManager::isExternalSlot(int ps, int ss) const
{
	for (int slot = 0; slot < 16; ++slot) {
		if (slots[slot].exists &&
		    (slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			return true;
		}
	}
	return false;
}

} // namespace openmsx
