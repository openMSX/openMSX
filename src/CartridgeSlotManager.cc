#include "CartridgeSlotManager.hh"
#include "HardwareConfig.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "TclObject.hh"
#include "MSXException.hh"
#include "MSXCPUInterface.hh"
#include "MSXRom.hh"
#include "MSXCliComm.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <array>
#include <cassert>
#include <memory>

using std::string;

namespace openmsx {

// CartridgeSlotManager::Slot
CartridgeSlotManager::Slot::~Slot()
{
	assert(!config);
	assert(useCount == 0);
}

bool CartridgeSlotManager::Slot::exists() const
{
	return cartCommand.has_value();
}

bool CartridgeSlotManager::Slot::used(const HardwareConfig* allowed) const
{
	assert((useCount == 0) == (config == nullptr));
	return config && (config != allowed);
}

void CartridgeSlotManager::Slot::getMediaInfo(TclObject& result)
{
	if (config) {
		if (config->getType() == HardwareConfig::Type::EXTENSION) {
			// A 'true' extension, as specified in an XML file
			result.addDictKeyValues("target", config->getConfigName(),
			                        "devicename", config->getName(),
			                        "type", "extension");
		} else {
			assert(config->getType() == HardwareConfig::Type::ROM);
			result.addDictKeyValue("type", "rom");
			// A ROM cartridge, peek into the internal config for the original filename
			const auto& romConfig = config->getConfig()
				.getChild("devices").getChild("primary").getChild("secondary")
				.getChild("ROM").getChild("rom");
			result.addDictKeyValue("target", romConfig.getChildData("filename"));
			TclObject patches;
			if (const auto* patchesElem = romConfig.findChild("patches")) {
				for (const auto* p : patchesElem->getChildren("ips")) {
					patches.addListElement(p->getData());
				}
			}
			result.addDictKeyValue("patches", patches);
			if (const auto* rom = dynamic_cast<const MSXRom*>(cpuInterface->getMSXDevice(ps, ss < 0 ? 0: ss, 1))) {
				rom->getInfo(result);
			}
		}
	} else {
		result.addDictKeyValue("target", std::string_view{});
	}
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

int CartridgeSlotManager::getSlotNum(std::string_view slot)
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
	throw MSXException("Invalid slot specification: ", slot);
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
	for (auto slotNum : xrange(MAX_SLOTS)) {
		auto& slot = slots[slotNum];
		if (!slot.exists()) {
			slot.ps = ps;
			slot.ss = ss;

			std::array slotName = {'c','a','r','t','X','\0'};
			slotName[4] = narrow<char>('a' + slotNum);
			motherBoard.getMSXCliComm().update(
				CliComm::UpdateType::HARDWARE, slotName.data(), "add");
			slot.cartCommand.emplace(
				*this, motherBoard, slotName.data());
			motherBoard.registerMediaInfo(
				slot.cartCommand->getName(), slot);

			std::array extName = {'e','x','t','X','\0'};
			extName[3] = narrow<char>('a' + slotNum);
			slot.extCommand.emplace(
				motherBoard, extName.data());
			slot.cpuInterface = &motherBoard.getCPUInterface();
			return;
		}
	}
	UNREACHABLE;
}


unsigned CartridgeSlotManager::getSlot(int ps, int ss) const
{
	for (auto slot : xrange(MAX_SLOTS)) {
		if (slots[slot].exists() &&
		    (slots[slot].ps == ps) && (slots[slot].ss == ss)) {
			return slot;
		}
	}
	UNREACHABLE; // was not an external slot
}

void CartridgeSlotManager::testRemoveExternalSlot(
	int ps, const HardwareConfig& allowed) const
{
	testRemoveExternalSlot(ps, -1, allowed);
}

void CartridgeSlotManager::testRemoveExternalSlot(
	int ps, int ss, const HardwareConfig& allowed) const
{
	auto slot = getSlot(ps, ss);
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
	auto slotNum = getSlot(ps, ss);
	auto& slot = slots[slotNum];
	assert(!slot.used());
	motherBoard.unregisterMediaInfo(slot);
	assert(slot.cartCommand);
	motherBoard.getMSXCliComm().update(
		CliComm::UpdateType::HARDWARE, slot.cartCommand->getName(), "remove");
	slot.cartCommand.reset();
	slot.extCommand.reset();
}

void CartridgeSlotManager::getSpecificSlot(unsigned slot, int& ps, int& ss) const
{
	assert(slot < MAX_SLOTS);
	if (!slots[slot].exists()) {
		throw MSXException("slot-", char('a' + slot), " not defined.");
	}
	if (slots[slot].used()) {
		throw MSXException("slot-", char('a' + slot), " already in use.");
	}
	ps = slots[slot].ps;
	ss = slots[slot].ss;
}

int CartridgeSlotManager::allocateSpecificPrimarySlot(unsigned slot, const HardwareConfig& hwConfig)
{
	assert(slot < MAX_SLOTS);
	if (!slots[slot].exists()) {
		throw MSXException("slot-", char('a' + slot), " not defined.");
	}
	if (slots[slot].used()) {
		throw MSXException("slot-", char('a' + slot), " already in use.");
	}
	if (slots[slot].ss != -1) {
		throw MSXException("slot-", char('a' + slot), " is not a primary slot.");
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
	auto slot = getSlot(ps, -1);
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
					throw MSXException(
						"Slot ", ps, '-', ss,
						" already in use by ",
						slots[slot].config->getName());
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
	return ranges::any_of(xrange(MAX_SLOTS), [&](auto slot) {
		int tmp = (convert && (slots[slot].ss == -1)) ? 0 : slots[slot].ss;
		return slots[slot].exists() && (slots[slot].ps == ps) && (tmp == ss);
	});
}


// CartCmd
CartridgeSlotManager::CartCmd::CartCmd(
		CartridgeSlotManager& manager_, MSXMotherBoard& motherBoard_,
		std::string_view commandName)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  commandName)
	, manager(manager_)
	, cliComm(motherBoard_.getMSXCliComm())
{
}

const HardwareConfig* CartridgeSlotManager::CartCmd::getExtensionConfig(
	std::string_view cartName) const
{
	if (cartName.size() != 5) {
		throw SyntaxError();
	}
	return manager.getConfigForSlot(cartName[4] - 'a');
}

void CartridgeSlotManager::CartCmd::execute(
	std::span<const TclObject> tokens, TclObject& result, EmuTime::param /*time*/)
{
	std::string_view cartName = tokens[0].getString();

	// strip namespace qualification
	//  TODO investigate whether it's a good idea to strip namespace at a
	//       higher level for all commands. How does that interact with
	//       the event recording feature?
	if (auto pos = cartName.rfind("::"); pos != std::string_view::npos) {
		cartName = cartName.substr(pos + 2);
	}
	if (tokens.size() == 1) {
		// query name of cartridge
		const auto* extConf = getExtensionConfig(cartName);
		result.addListElement(tmpStrCat(cartName, ':'),
		                      extConf ? extConf->getName() : string{});
		if (!extConf) {
			TclObject options = makeTclList("empty");
			result.addListElement(options);
		}
	} else if (tokens[1] == one_of("eject", "-eject")) {
		// remove cartridge (or extension)
		if (tokens[1] == "-eject") {
			result =
				"Warning: use of '-eject' is deprecated, "
				"instead use the 'eject' subcommand";
		}
		if (const auto* extConf = getExtensionConfig(cartName)) {
			try {
				manager.motherBoard.removeExtension(*extConf);
				cliComm.update(CliComm::UpdateType::MEDIA, cartName, {});
			} catch (MSXException& e) {
				throw CommandException("Can't remove cartridge: ",
				                       e.getMessage());
			}
		}
	} else {
		// insert cartridge
		auto slotName = (cartName.size() == 5)
			? cartName.substr(4, 1)
			: "any";
		size_t extensionNameToken = 1;
		if (tokens[1] == "insert") {
			if (tokens.size() > 2) {
				extensionNameToken = 2;
			} else {
				throw CommandException("Missing argument to insert subcommand");
			}
		}
		auto options = tokens.subspan(extensionNameToken + 1);
		try {
			std::string_view romName = tokens[extensionNameToken].getString();
			auto extension = HardwareConfig::createRomConfig(
				manager.motherBoard, romName, slotName, options);
			if (slotName != "any") {
				if (const auto* extConf = getExtensionConfig(cartName)) {
					// still a cartridge inserted, (try to) remove it now
					manager.motherBoard.removeExtension(*extConf);
				}
			}
			result = manager.motherBoard.insertExtension(
				"ROM", std::move(extension));
			cliComm.update(CliComm::UpdateType::MEDIA, cartName, romName);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}
	}
}

string CartridgeSlotManager::CartCmd::help(std::span<const TclObject> tokens) const
{
	auto cart = tokens[0].getString();
	return strCat(
		cart, " eject              : remove the ROM cartridge from this slot\n",
		cart, " insert <filename>  : insert ROM cartridge with <filename>\n",
		cart, " <filename>         : insert ROM cartridge with <filename>\n",
		cart, "                    : show which ROM cartridge is in this slot\n",
		"The following options are supported when inserting a cartridge:\n"
		"-ips <filename>    : apply the given IPS patch to the ROM image\n"
		"-romtype <romtype> : specify the ROM mapper type\n");
}

void CartridgeSlotManager::CartCmd::tabCompletion(std::vector<string>& tokens) const
{
	using namespace std::literals;
	static constexpr std::array extra = {"eject"sv, "insert"sv};
	completeFileName(tokens, userFileContext(),
	                 (tokens.size() < 3) ? extra : std::span<const std::string_view>{});

}

bool CartridgeSlotManager::CartCmd::needRecord(std::span<const TclObject> tokens) const
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
	std::span<const TclObject> tokens, TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?slot?");
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
		if ((slotName.size() != 5) || !slotName.starts_with("slot")) {
			throw CommandException("Invalid slot name: ", slotName);
		}
		unsigned num = slotName[4] - 'a';
		if (num >= CartridgeSlotManager::MAX_SLOTS) {
			throw CommandException("Invalid slot name: ", slotName);
		}
		auto& slot = manager.slots[num];
		if (!slot.exists()) {
			throw CommandException("Slot '", slotName, "' doesn't currently exist in this msx machine.");
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
			result.addListElement(std::string_view{});
		}
		break;
	}
	}
}

string CartridgeSlotManager::CartridgeSlotInfo::help(
	std::span<const TclObject> /*tokens*/) const
{
	return "Without argument: show list of available external slots.\n"
	       "With argument: show primary and secondary slot number for "
	       "given external slot.\n";
}

} // namespace openmsx
