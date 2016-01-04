#include "CartridgeSlotManager.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "TclObject.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "CliComm.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "outer.hh"
#include "xrange.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

// CartridgeSlotManager::Slot
CartridgeSlotManager::Slot::Slot()
	: config(nullptr), useCount(0), ps(0), ss(0)
{
}

CartridgeSlotManager::Slot::~Slot()
{
	assert(!config);
	assert(useCount == 0);
}

bool CartridgeSlotManager::Slot::exists() const
{
	return cartCommand != nullptr;
}

bool CartridgeSlotManager::Slot::used(const HardwareConfig* allowed) const
{
	assert((useCount == 0) == (config == nullptr));
	return config && (config != allowed);
}


// CartridgeSlotManager
CartridgeSlotManager::CartridgeSlotManager(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, cartCmd(*this, motherBoard, "cart")
	, extSlotInfo(motherBoard.getMachineInfoCommand())
{
}

CartridgeSlotManager::~CartridgeSlotManager()
{
	for (auto slot : xrange(MAX_SLOTS)) {
		(void)slot;
		assert(!slots[slot].exists());
		assert(!slots[slot].used());
	}
}

int CartridgeSlotManager::getSlotNum(string_ref slot)
{
	if (slot.size() == 1) {
		if (('0' <= slot[0]) && (slot[0] <= '3')) {
			return slot[0] - '0';
		} else if (('a' <= slot[0]) && (slot[0] <= 'p')) {
			return -(1 + slot[0] - 'a');
		} else if (slot[0] == 'X') {
			return -256;
		}
	} else if (slot.size() == 2) {
		if ((slot[0] == '?') && ('0' <= slot[1]) && (slot[1] <= '3')) {
			return slot[1] - '0' - 128;
		}
	} else if (slot == "any") {
		return -256;
	}
	throw MSXException("Invalid slot specification: " + slot);
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
	for (auto slot : xrange(MAX_SLOTS)) {
		if (!slots[slot].exists()) {
			slots[slot].ps = ps;
			slots[slot].ss = ss;
			char slotName[] = "carta";
			slotName[4] += slot;
			char extName[] = "exta";
			extName[3] += slot;
			motherBoard.getMSXCliComm().update(
				CliComm::HARDWARE, slotName, "add");
			slots[slot].cartCommand = make_unique<CartCmd>(
				*this, motherBoard, slotName);
			slots[slot].extCommand = make_unique<ExtCmd>(
				motherBoard, extName);
			return;
		}
	}
	UNREACHABLE;
}


int CartridgeSlotManager::getSlot(int ps, int ss) const
{
	for (auto slot : xrange(MAX_SLOTS)) {
		if (slots[slot].exists() &&
		    (slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			return slot;
		}
	}
	UNREACHABLE; // was not an external slot
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
	const auto& slotName = slots[slot].cartCommand->getName();
	motherBoard.getMSXCliComm().update(
		CliComm::HARDWARE, slotName, "remove");
	slots[slot].cartCommand.reset();
	slots[slot].extCommand.reset();
}

void CartridgeSlotManager::getSpecificSlot(unsigned slot, int& ps, int& ss) const
{
	assert(slot < MAX_SLOTS);
	if (!slots[slot].exists()) {
		throw MSXException(StringOp::Builder() <<
			"slot-" << char('a' + slot) << " not defined.");
	}
	if (slots[slot].used()) {
		throw MSXException(StringOp::Builder() <<
			"slot-" << char('a' + slot) << " already in use.");
	}
	ps = slots[slot].ps;
	ss = slots[slot].ss;
}

int CartridgeSlotManager::allocateSpecificPrimarySlot(unsigned slot, const HardwareConfig& hwConfig)
{
	assert(slot < MAX_SLOTS);
	if (!slots[slot].exists()) {
		throw MSXException(StringOp::Builder() <<
			"slot-" << char('a' + slot) << " not defined.");
	}
	if (slots[slot].used()) {
		throw MSXException(StringOp::Builder() <<
			"slot-" << char('a' + slot) << " already in use.");
	}
	if (slots[slot].ss != -1) {
		throw MSXException(StringOp::Builder() <<
			"slot-" << char('a' + slot) << " is not a primary slot.");
	}
	assert(slots[slot].useCount == 0);
	slots[slot].config = &hwConfig;
	slots[slot].useCount = 1;
	return slots[slot].ps;
}

void CartridgeSlotManager::getAnyFreeSlot(int& ps, int& ss) const
{
	// search for the lowest free slot
	ps = 4; // mark no free slot
	for (auto slot : xrange(MAX_SLOTS)) {
		if (slots[slot].exists() && !slots[slot].used()) {
			int p = slots[slot].ps;
			int s = slots[slot].ss;
			if ((p < ps) || ((p == ps) && (s < ss))) {
				ps = p;
				ss = s;
			}
		}
	}
	if (ps == 4) {
		throw MSXException("Not enough free cartridge slots");
	}
}

int CartridgeSlotManager::allocateAnyPrimarySlot(const HardwareConfig& hwConfig)
{
	for (auto slot : xrange(MAX_SLOTS)) {
		if (slots[slot].exists() && (slots[slot].ss == -1) &&
		    !slots[slot].used()) {
			assert(slots[slot].useCount == 0);
			slots[slot].config = &hwConfig;
			slots[slot].useCount = 1;
			return slots[slot].ps;
		}
	}
	throw MSXException("No free primary slot");
}

void CartridgeSlotManager::freePrimarySlot(
		int ps, const HardwareConfig& hwConfig)
{
	int slot = getSlot(ps, -1);
	assert(slots[slot].config == &hwConfig); (void)hwConfig;
	assert(slots[slot].useCount == 1);
	slots[slot].config = nullptr;
	slots[slot].useCount = 0;
}

void CartridgeSlotManager::allocateSlot(
		int ps, int ss, const HardwareConfig& hwConfig)
{
	for (auto slot : xrange(MAX_SLOTS)) {
		if (!slots[slot].exists()) continue;
		if ((slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			if (slots[slot].useCount == 0) {
				slots[slot].config = &hwConfig;
			} else {
				if (slots[slot].config != &hwConfig) {
					throw MSXException(StringOp::Builder()
						<< "Slot " << ps << '-' << ss
						<< " already in use by "
						<< slots[slot].config->getName());
				}
			}
			++slots[slot].useCount;
		}
	}
	// Slot not found, was not an external slot. No problem.
}

void CartridgeSlotManager::freeSlot(
		int ps, int ss, const HardwareConfig& hwConfig)
{
	for (auto slot : xrange(MAX_SLOTS)) {
		if (!slots[slot].exists()) continue;
		if ((slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			assert(slots[slot].config == &hwConfig); (void)hwConfig;
			assert(slots[slot].useCount > 0);
			--slots[slot].useCount;
			if (slots[slot].useCount == 0) {
				slots[slot].config = nullptr;
			}
			return;
		}
	}
	// Slot not found, was not an external slot. No problem.
}

bool CartridgeSlotManager::isExternalSlot(int ps, int ss, bool convert) const
{
	for (auto slot : xrange(MAX_SLOTS)) {
		int tmp = (convert && (slots[slot].ss == -1)) ? 0 : slots[slot].ss;
		if (slots[slot].exists() &&
		    (slots[slot].ps == ps) && (tmp == ss)) {
			return true;
		}
	}
	return false;
}


// CartCmd
CartridgeSlotManager::CartCmd::CartCmd(
		CartridgeSlotManager& manager_, MSXMotherBoard& motherBoard_,
		string_ref commandName)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  commandName)
	, manager(manager_)
	, cliComm(motherBoard_.getMSXCliComm())
{
}

const HardwareConfig* CartridgeSlotManager::CartCmd::getExtensionConfig(
	string_ref cartname)
{
	if (cartname.size() != 5) {
		throw SyntaxError();
	}
	int slot = cartname[4] - 'a';
	return manager.slots[slot].config;
}

void CartridgeSlotManager::CartCmd::execute(
	array_ref<TclObject> tokens, TclObject& result, EmuTime::param /*time*/)
{
	string_ref cartname = tokens[0].getString();

	// strip namespace qualification
	//  TODO investigate whether it's a good idea to strip namespace at a
	//       higher level for all commands. How does that interact with
	//       the event recording feature?
	auto pos = cartname.rfind("::");
	if (pos != string_ref::npos) {
		cartname = cartname.substr(pos + 2);
	}
	if (tokens.size() == 1) {
		// query name of cartridge
		auto* extConf = getExtensionConfig(cartname);
		result.addListElement(cartname + ':');
		result.addListElement(extConf ? extConf->getName() : "");
		if (!extConf) {
			TclObject options;
			options.addListElement("empty");
			result.addListElement(options);
		}
	} else if ((tokens[1] == "eject") || (tokens[1] == "-eject")) {
		// remove cartridge (or extension)
		if (tokens[1] == "-eject") {
			result.setString(
				"Warning: use of '-eject' is deprecated, "
				"instead use the 'eject' subcommand");
		}
		if (auto* extConf = getExtensionConfig(cartname)) {
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
		auto slotname = (cartname.size() == 5)
			? string(1, cartname[4])
			: "any";
		size_t extensionNameToken = 1;
		if (tokens[1] == "insert") {
			if (tokens.size() > 2) {
				extensionNameToken = 2;
			} else {
				throw CommandException("Missing argument to insert subcommand");
			}
		}
		array_ref<TclObject> options(std::begin(tokens) + extensionNameToken + 1,
		                             std::end(tokens));
		try {
			string_ref romname = tokens[extensionNameToken].getString();
			auto extension = HardwareConfig::createRomConfig(
				manager.motherBoard, romname, slotname, options);
			if (slotname != "any") {
				if (auto* extConf = getExtensionConfig(cartname)) {
					// still a cartridge inserted, (try to) remove it now
					manager.motherBoard.removeExtension(*extConf);
				}
			}
			result.setString(manager.motherBoard.insertExtension(
				"ROM", std::move(extension)));
			cliComm.update(CliComm::MEDIA, cartname, romname);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	}
}

string CartridgeSlotManager::CartCmd::help(const vector<string>& tokens) const
{
	return tokens[0] + " eject              : remove the ROM cartridge from this slot\n" +
	       tokens[0] + " insert <filename>  : insert ROM cartridge with <filename>\n" +
	       tokens[0] + " <filename>         : insert ROM cartridge with <filename>\n" +
	       tokens[0] + "                    : show which ROM cartridge is in this slot\n" +
	       "The following options are supported when inserting a cartridge:\n" +
	       "-ips <filename>    : apply the given IPS patch to the ROM image\n" +
	       "-romtype <romtype> : specify the ROM mapper type\n";
}

void CartridgeSlotManager::CartCmd::tabCompletion(vector<string>& tokens) const
{
	vector<const char*> extra;
	if (tokens.size() < 3) {
		extra = { "eject", "insert" };
	}
	completeFileName(tokens, userFileContext(), extra);
}

bool CartridgeSlotManager::CartCmd::needRecord(array_ref<TclObject> tokens) const
{
	return tokens.size() > 1;
}


// class CartridgeSlotInfo

CartridgeSlotManager::CartridgeSlotInfo::CartridgeSlotInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "external_slot")
{
}

void CartridgeSlotManager::CartridgeSlotInfo::execute(
	array_ref<TclObject> tokens, TclObject& result) const
{
	auto& manager = OUTER(CartridgeSlotManager, extSlotInfo);
	switch (tokens.size()) {
	case 2: {
		// return list of slots
		string slot = "slotX";
		for (auto i : xrange(CartridgeSlotManager::MAX_SLOTS)) {
			if (!manager.slots[i].exists()) continue;
			slot[4] = char('a' + i);
			result.addListElement(slot);
		}
		break;
	}
	case 3: {
		// return info on a particular slot
		const auto& slotName = tokens[2].getString();
		if ((slotName.size() != 5) || (!slotName.starts_with("slot"))) {
			throw CommandException("Invalid slot name: " + slotName);
		}
		unsigned num = slotName[4] - 'a';
		if (num >= CartridgeSlotManager::MAX_SLOTS) {
			throw CommandException("Invalid slot name: " + slotName);
		}
		auto& slot = manager.slots[num];
		if (!slot.exists()) {
			throw CommandException("Slot '" + slotName + "' doesn't currently exist in this msx machine.");
		}
		result.addListElement(slot.ps);
		if (slot.ss == -1) {
			result.addListElement("X");
		} else {
			result.addListElement(slot.ss);
		}
		if (slot.config) {
			result.addListElement(slot.config->getName());
		} else {
			result.addListElement("");
		}
		break;
	}
	default:
		throw SyntaxError();
	}
}

string CartridgeSlotManager::CartridgeSlotInfo::help(
	const vector<string>& /*tokens*/) const
{
	return "Without argument: show list of available external slots.\n"
	       "With argument: show primary and secondary slot number for "
	       "given external slot.\n";
}

} // namespace openmsx
