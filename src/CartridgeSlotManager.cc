// $Id$

#include <cassert>
#include "CartridgeSlotManager.hh"
#include "MSXConfig.hh"
#include "Config.hh"


namespace openmsx {

const int RESERVED = 16;
const int EXISTS   = 32;


CartridgeSlotManager::CartridgeSlotManager()
{
	for (int slot = 0; slot < 16; slot++) {
		slots[slot] = 0;
	}
}

CartridgeSlotManager* CartridgeSlotManager::instance()
{
	static CartridgeSlotManager oneInstance;
	return &oneInstance;
}


void CartridgeSlotManager::reserveSlot(int slot)
{
	assert((0 <= slot) && (slot < 16));
	slots[slot] |= RESERVED;
}

void CartridgeSlotManager::readConfig()
{
	try {
		Config* config = MSXConfig::instance()->
					       getConfigById("ExternalSlots");
		string slotName("slota");
		for (int slot = 0; slot < 16; slot++) {
			slotName[4] = 'a' + slot;
			if (config->hasParameter(slotName)) {
				string slotValue = config->getParameter(slotName);
				int ps = slotValue[0] - '0';
				int ss = slotValue[2] - '0';
				if ((ps < 0) || (ps > 3) || (ss < 0) || (ss > 3) ||
				    (slotValue[1] != '-') || (slotValue.length() != 3)) {
					throw FatalError("Syntax error in ExternalSlots config");
				}
				slots[slot] |= (ss << 2) | ps | EXISTS;
			}
		}
	} catch (ConfigException &e) {
		// do nothing
	}
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
