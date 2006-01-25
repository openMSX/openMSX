// $Id$

#include "CartridgeSlotManager.hh"
#include "MSXMotherBoard.hh"
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
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		assert(!slots[slot].exists);
		assert(!slots[slot].used);
		assert(!slots[slot].reserved);
	}
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
			throw MSXException(
				"Invalid slot specification: " + slot);
		}
		return result;
	}
}

void CartridgeSlotManager::reserveSlot(int slot)
{
	assert((0 <= slot) && (slot < MAX_SLOTS));
	slots[slot].reserved++;
}

void CartridgeSlotManager::unreserveSlot(int slot)
{
	assert((0 <= slot) && (slot < MAX_SLOTS));
	assert(slots[slot].reserved != 0);
	slots[slot].reserved--;
}

void CartridgeSlotManager::createExternalSlot(int ps)
{
	createExternalSlot(ps, -1);
}

void CartridgeSlotManager::createExternalSlot(int ps, int ss)
{
	if (isExternalSlot(ps, ss, false)) {
		throw MSXException("Slot is already an external slot.");
	}
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		if (!slots[slot].exists) {
			slots[slot].ps = ps;
			slots[slot].ss = ss;
			slots[slot].exists = true;
			return;
		}
	}
	assert(false);
}

void CartridgeSlotManager::removeExternalSlot(int ps)
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		if (slots[slot].exists &&
		    (slots[slot].ps == ps) && (slots[slot].ss == -1)) {
			slots[slot].exists = false;
			return;
		}
	}
	assert(false); // was not an external slot
}

void CartridgeSlotManager::removeExternalSlot(int ps, int ss)
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		if (slots[slot].exists &&
		    (slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			slots[slot].exists = false;
			return;
		}
	}
	assert(false); // was not an external slot
}

int CartridgeSlotManager::getReservedSlot(int slot, int& ps, int& ss)
{
	assert((0 <= slot) && (slot < MAX_SLOTS));
	assert(slots[slot].reserved);

	if (!slots[slot].exists) {
		throw MSXException(string("Slot") + (char)('a' + slot) +
		                   " not defined");
	}
	ps = slots[slot].ps;
	ss = (slots[slot].ss != -1) ? slots[slot].ss : 0;
	slots[slot].used = true;
	return slot;
}

int CartridgeSlotManager::getAnyFreeSlot(int& ps, int& ss)
{
	for (int slot = 0; slot < MAX_SLOTS; slot++) {
		if (slots[slot].exists &&
		    !slots[slot].reserved && !slots[slot].used) {
			slots[slot].used = true;
			ps = slots[slot].ps;
			ss = (slots[slot].ss != -1) ? slots[slot].ss : 0;
			return slot;
		}
	}
	throw MSXException("Not enough free cartridge slots");
}

int CartridgeSlotManager::getFreePrimarySlot(int &ps)
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		ps = slots[slot].ps;
		if (slots[slot].exists && (slots[slot].ss == -1) &&
		    !slots[slot].reserved && !slots[slot].used) {
			slots[slot].used = true;
			return slot;
		}
	}
	throw MSXException("No free primary slot");
}

void CartridgeSlotManager::freeSlot(int slot)
{
	assert(slots[slot].used);
	slots[slot].used = false;
}

bool CartridgeSlotManager::isExternalSlot(int ps, int ss, bool convert) const
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		int tmp = (convert && (slots[slot].ss == -1)) ? 0 : slots[slot].ss;
		if (slots[slot].exists &&
		    (slots[slot].ps == ps) && (tmp == ss)) {
			return true;
		}
	}
	return false;
}

} // namespace openmsx
