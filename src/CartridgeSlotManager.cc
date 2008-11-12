// $Id$

#include "CartridgeSlotManager.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "RecordedCommand.hh"
#include "CommandException.hh"
#include "CommandController.hh"
#include "FileContext.hh"
#include "TclObject.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include "CliComm.hh"
#include <cassert>

using std::string;
using std::vector;
using std::set;

namespace openmsx {

class CartCmd : public RecordedCommand
{
public:
	CartCmd(CartridgeSlotManager& manager, MSXMotherBoard& motherBoard,
	        const std::string& commandName);
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	const HardwareConfig* getExtensionConfig(const string& cartname);
	CartridgeSlotManager& manager;
	CliComm& cliComm;
};


// CartridgeSlotManager::Slot
CartridgeSlotManager::Slot::Slot()
	: ps(0), ss(0), config(NULL)
{
}

CartridgeSlotManager::Slot::~Slot()
{
}

bool CartridgeSlotManager::Slot::exists() const
{
	return command.get();
}

bool CartridgeSlotManager::Slot::used(const HardwareConfig* allowed) const
{
	return config && (config != allowed);
}


// CartridgeSlotManager
CartridgeSlotManager::CartridgeSlotManager(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, cartCmd(new CartCmd(*this, motherBoard, "cart"))
{
}

CartridgeSlotManager::~CartridgeSlotManager()
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		assert(!slots[slot].exists());
		assert(!slots[slot].used());
	}
}

int CartridgeSlotManager::getSlotNum(const string& slot)
{
	if ((slot.size() == 1) && ('a' <= slot[0]) && (slot[0] <= 'p')) {
		return -(1 + slot[0] - 'a');
	} else if (slot == "any") {
		return -256;
	} else if ((slot.size() == 2) && (slot[0] == '?')) {
		int result = slot[1] - '0';
		if ((result < 0) || (4 <= result)) {
			throw MSXException(
				"Invalid slot specification: " + slot);
		}
		return result-128;
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
			string slotName = "carta";
			slotName[4] += slot;
			motherBoard.getMSXCliComm().update(
				CliComm::HARDWARE, slotName, "add");
			slots[slot].command.reset(
				new CartCmd(*this, motherBoard, slotName));
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

void CartridgeSlotManager::testRemoveExternalSlot(
	int ps, const HardwareConfig& allowed) const
{
	testRemoveExternalSlot(ps, -1, allowed);
}

void CartridgeSlotManager::testRemoveExternalSlot(
	int ps, int ss, const HardwareConfig& allowed) const
{
	int slot = getSlot(ps, ss);
	if (slots[slot].used(&allowed)) {
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
	assert(!slots[slot].used());
	const string& slotName = slots[slot].command->getName();
	motherBoard.getMSXCliComm().update(
		CliComm::HARDWARE, slotName, "remove");
	slots[slot].command.reset();
}

int CartridgeSlotManager::getSpecificSlot(int slot, int& ps, int& ss,
                                          const HardwareConfig& hwConfig)
{
	assert((0 <= slot) && (slot < MAX_SLOTS));

	if (!slots[slot].exists()) {
		throw MSXException(string("slot-") + char('a' + slot) +
		                   " not defined.");
	}
	if (slots[slot].used()) {
		throw MSXException(string("slot-") + char('a' + slot) +
		                   " already in use.");
	}
	ps = slots[slot].ps;
	ss = (slots[slot].ss != -1) ? slots[slot].ss : 0;
	slots[slot].config = &hwConfig;
	return slot;
}

int CartridgeSlotManager::getAnyFreeSlot(int& ps, int& ss,
                                         const HardwareConfig& hwConfig)
{
	// search for the lowest free slot
	int result = -1;
	unsigned slotNum = unsigned(-1);
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		if (slots[slot].exists() && !slots[slot].used()) {
			unsigned p = slots[slot].ps;
			unsigned s = (slots[slot].ss != -1) ? slots[slot].ss : 0;
			assert((p < 4) && (s < 4));
			unsigned t = p * 4 + s;
			if (t < slotNum) {
				slotNum = t;
				result = slot;
			}
		}
	}
	if (result == -1) {
		throw MSXException("Not enough free cartridge slots");
	}
	slots[result].config = &hwConfig;
	ps = slotNum / 4;
	ss = slotNum % 4;
	return result;
}

int CartridgeSlotManager::getFreePrimarySlot(
		int& ps, const HardwareConfig& hwConfig)
{
	for (int slot = 0; slot < MAX_SLOTS; ++slot) {
		ps = slots[slot].ps;
		if (slots[slot].exists() && (slots[slot].ss == -1) &&
		    !slots[slot].used()) {
			slots[slot].config = &hwConfig;
			return slot;
		}
	}
	throw MSXException("No free primary slot");
}

void CartridgeSlotManager::freeSlot(int slot)
{
	assert(slots[slot].used());
	slots[slot].config = NULL;
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
CartCmd::CartCmd(CartridgeSlotManager& manager_, MSXMotherBoard& motherBoard,
                 const string& commandName)
	: RecordedCommand(motherBoard.getCommandController(),
	                  motherBoard.getMSXEventDistributor(),
	                  motherBoard.getScheduler(),
	                  commandName)
	, manager(manager_)
	, cliComm(motherBoard.getMSXCliComm())
{
}

const HardwareConfig* CartCmd::getExtensionConfig(const string& cartname)
{
	if (cartname.size() != 5) {
		throw SyntaxError();
	}
	int slot = cartname[4] - 'a';
	return manager.slots[slot].config;
}

string CartCmd::execute(const vector<string>& tokens, EmuTime::param /*time*/)
{
	string result;
	string cartname = tokens[0];

	// strip namespace qualification
	//  TODO investigate whether it's a good idea to strip namespace at a
	//       higher level for all commands. How does that interact with
	//       the event recording feature?
	string::size_type pos = cartname.rfind("::");
	if (pos != string::npos) {
		cartname = cartname.substr(pos + 2);
	}
	if (tokens.size() == 1) {
		// query name of cartridge
		const HardwareConfig* extConf = getExtensionConfig(cartname);
		TclObject object(getCommandController().getInterpreter());
		object.addListElement(cartname + ':');
		object.addListElement(extConf ? extConf->getName() : "");
		TclObject options(getCommandController().getInterpreter());
		if (!extConf) {
			options.addListElement("empty");
		}
		if (options.getListLength() != 0) {
			object.addListElement(options);
		}
		result = object.getString();
	} else if ( (tokens[1] == "eject") || (tokens[1] == "-eject") ) {
		// remove cartridge (or extension)
		if (tokens[1] == "-eject") {
			result += "Warning: use of '-eject' is deprecated, instead use the 'eject' subcommand";
		}
		if (const HardwareConfig* extConf = getExtensionConfig(cartname)) {
			try {
				manager.motherBoard.removeExtension(*extConf);
				cliComm.update(CliComm::MEDIA, cartname, "");
			} catch (MSXException& e) {
				throw CommandException("Can't remove cartridge: " +
				                       e.getMessage());
			}
		}

	} else {
		// insert cartridge
		string slotname = (cartname.size() == 5)
			? string(1, cartname[4])
			: "any";
		int extensionNameToken = 1;
		if (tokens[1] == "insert") {
			if (tokens.size() > 2) {
				extensionNameToken = 2;
			} else {
				throw CommandException("Missing argument to insert subcommand");
			}
		}
		vector<string> options;
		for (unsigned i = (extensionNameToken + 1); i < tokens.size(); ++i) {
			options.push_back(tokens[i]);
		}
		try {
			if (const HardwareConfig* extConf =
			               getExtensionConfig(cartname)) {
				// still a cartridge inserted, (try to) remove it now
				manager.motherBoard.removeExtension(*extConf);
			}
			HardwareConfig& extension =
				manager.motherBoard.loadRom(
					tokens[extensionNameToken], slotname, options);
			cliComm.update(CliComm::MEDIA, cartname, tokens[extensionNameToken]);
			result = extension.getName();
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	}
	return result;
}

string CartCmd::help(const vector<string>& tokens) const
{
	return tokens[0] + " eject              : remove the ROM cartridge from this slot\n" +
	       tokens[0] + " insert <filename>  : insert ROM cartridge with <filename>\n" +
	       tokens[0] + " <filename>         : insert ROM cartridge with <filename>\n" +
	       tokens[0] + "                    : show which ROM cartridge is in this slot";
}

void CartCmd::tabCompletion(vector<string>& tokens) const
{
	set<string> extra;
	if (tokens.size() < 3) {
		extra.insert("eject");
		extra.insert("insert");
	}
	UserFileContext context;
	completeFileName(getCommandController(), tokens, context, extra);
}

} // namespace openmsx
