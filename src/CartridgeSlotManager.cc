// $Id$

#include "CartridgeSlotManager.hh"
#include "MSXMotherBoard.hh"
#include "ExtensionConfig.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class CartCmd : public SimpleCommand
{
public:
	CartCmd(MSXMotherBoard& motherBoard, const std::string& commandName);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoard& motherBoard;
};


CartridgeSlotManager::CartridgeSlotManager(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, cartCmd(new CartCmd(motherBoard, "cart"))
{
}

CartridgeSlotManager::~CartridgeSlotManager()
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		assert(!slots[slot].exists());
		assert(!slots[slot].used);
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
		if (!slots[slot].exists()) {
			slots[slot].ps = ps;
			slots[slot].ss = ss;
			string slotname = "carta";
			slotname[4] += slot;
			slots[slot].command = new CartCmd(motherBoard, slotname);
			return;
		}
	}
	assert(false);
}


int CartridgeSlotManager::getSlot(int ps, int ss) const
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		if (slots[slot].exists() &&
		    (slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			return slot;
		}
	}
	assert(false); // was not an external slot
	return 0; // avoid warning
}

void CartridgeSlotManager::testRemoveExternalSlot(int ps) const
{
	testRemoveExternalSlot(ps, -1);
}

void CartridgeSlotManager::testRemoveExternalSlot(int ps, int ss) const
{
	int slot = getSlot(ps, ss);
	if (slots[slot].used) {
		throw MSXException("Slot still in use.");
	}
}

void CartridgeSlotManager::removeExternalSlot(int ps)
{
	removeExternalSlot(ps, -1);
}

void CartridgeSlotManager::removeExternalSlot(int ps, int ss)
{
	int slot = getSlot(ps, ss);
	assert(!slots[slot].used);
	delete slots[slot].command;
	slots[slot].command = NULL;
}

int CartridgeSlotManager::getSpecificSlot(int slot, int& ps, int& ss)
{
	assert((0 <= slot) && (slot < MAX_SLOTS));

	if (!slots[slot].exists()) {
		throw MSXException(string("slot-") + (char)('a' + slot) +
		                   " not defined.");
	}
	if (slots[slot].used) {
		throw MSXException(string("slot-") + (char)('a' + slot) +
		                   " already in use.");
	}
	ps = slots[slot].ps;
	ss = (slots[slot].ss != -1) ? slots[slot].ss : 0;
	slots[slot].used = true;
	return slot;
}

int CartridgeSlotManager::getAnyFreeSlot(int& ps, int& ss)
{
	for (int slot = 0; slot < MAX_SLOTS; slot++) {
		if (slots[slot].exists() && !slots[slot].used) {
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
		if (slots[slot].exists() && (slots[slot].ss == -1) &&
		    !slots[slot].used) {
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
		if (slots[slot].exists() &&
		    (slots[slot].ps == ps) && (tmp == ss)) {
			return true;
		}
	}
	return false;
}


// CartCmd
CartCmd::CartCmd(MSXMotherBoard& motherBoard_, const string& commandName)
	: SimpleCommand(motherBoard_.getCommandController(), commandName)
	, motherBoard(motherBoard_)
{
}

string CartCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	try {
		string slotname;
		if (tokens[0].size() == 5) {
			slotname = tokens[0][4];
		} else {
			slotname = "any";
		}
		vector<string> options(tokens.begin() + 2, tokens.end());

		ExtensionConfig& extension =
			motherBoard.loadRom(tokens[1], slotname, options);
		return extension.getName();
	} catch (MSXException& e) {
		throw CommandException(e.getMessage());
	}
}

string CartCmd::help(const vector<string>& /*tokens*/) const
{
	return "Insert a ROM cartridge.";
}

void CartCmd::tabCompletion(vector<string>& tokens) const
{
	completeFileName(tokens);
}

} // namespace openmsx
