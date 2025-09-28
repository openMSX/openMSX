#include "MSXCommandController.hh"

#include "GlobalCommandController.hh"
#include "Interpreter.hh"

#include "Event.hh"
#include "MSXEventDistributor.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "Setting.hh"
#include "SettingsManager.hh"

#include "TemporaryString.hh"
#include "stl.hh"

#include <algorithm>
#include <iostream>
#include <memory>

namespace openmsx {

MSXCommandController::MSXCommandController(
		GlobalCommandController& globalCommandController_,
		Reactor& reactor_,
		MSXMotherBoard& motherboard_,
		MSXEventDistributor& msxEventDistributor_,
		const std::string& machineID_)
	: globalCommandController(globalCommandController_)
	, reactor(reactor_)
	, motherboard(motherboard_)
	, msxEventDistributor(msxEventDistributor_)
	, machineID(strCat("::", machineID_, "::"))
{
	globalCommandController.getInterpreter().createNamespace(machineID);

	machineInfoCommand.emplace(*this, "machine_info");
	machineInfoCommand->setAllowedInEmptyMachine(true);

	msxEventDistributor.registerEventListener(*this);
}

MSXCommandController::~MSXCommandController()
{
	msxEventDistributor.unregisterEventListener(*this);

	machineInfoCommand.reset();

	#ifndef NDEBUG
	for (const auto* c : commandMap) {
		std::cout << "Command not unregistered: " << c->getName() << '\n';
	}
	for (const auto* s : settings) {
		std::cout << "Setting not unregistered: " << s->getFullName() << '\n';
	}
	assert(commandMap.empty());
	assert(settings.empty());
	#endif

	globalCommandController.getInterpreter().deleteNamespace(machineID);
}

TemporaryString MSXCommandController::getFullName(std::string_view name)
{
	return tmpStrCat(machineID, name);
}

void MSXCommandController::registerCommand(Command& command, zstring_view str)
{
	assert(!hasCommand(str));
	assert(command.getName() == str);
	commandMap.insert_noDuplicateCheck(&command);

	auto fullname = getFullName(str);
	globalCommandController.registerCommand(command, fullname);
	globalCommandController.registerProxyCommand(str);

	command.setAllowedInEmptyMachine(false);
}

void MSXCommandController::unregisterCommand(Command& command, std::string_view str)
{
	assert(hasCommand(str));
	assert(command.getName() == str);
	commandMap.erase(str);

	globalCommandController.unregisterProxyCommand(str);
	auto fullname = getFullName(str);
	globalCommandController.unregisterCommand(command, fullname);
}

void MSXCommandController::registerCompleter(CommandCompleter& completer,
                                             std::string_view str)
{
	auto fullname = getFullName(str);
	globalCommandController.registerCompleter(completer, fullname);
}

void MSXCommandController::unregisterCompleter(CommandCompleter& completer,
                                               std::string_view str)
{
	auto fullname = getFullName(str);
	globalCommandController.unregisterCompleter(completer, fullname);
}

void MSXCommandController::registerSetting(Setting& setting)
{
	setting.setPrefix(machineID);

	settings.push_back(&setting);

	globalCommandController.registerProxySetting(setting);
	globalCommandController.getSettingsManager().registerSetting(setting);
	globalCommandController.getInterpreter().registerSetting(setting);
}

void MSXCommandController::unregisterSetting(Setting& setting)
{
	move_pop_back(settings, rfind_unguarded(settings, &setting));

	globalCommandController.unregisterProxySetting(setting);
	globalCommandController.getInterpreter().unregisterSetting(setting);
	globalCommandController.getSettingsManager().unregisterSetting(setting);
}

Setting* MSXCommandController::findSetting(std::string_view name) const
{
	auto it = std::ranges::find(settings, name, &Setting::getBaseName);
	return it != settings.end() ? *it : nullptr;
}

Command* MSXCommandController::findCommand(std::string_view name) const
{
	auto it = commandMap.find(name);
	return (it != end(commandMap)) ? *it : nullptr;
}

bool MSXCommandController::hasCommand(std::string_view command) const
{
	return findCommand(command) != nullptr;
}

TclObject MSXCommandController::executeCommand(zstring_view command,
                                               CliConnection* connection)
{
	return globalCommandController.executeCommand(command, connection);
}

MSXCliComm& MSXCommandController::getCliComm()
{
	return motherboard.getMSXCliComm();
}

Interpreter& MSXCommandController::getInterpreter()
{
	return globalCommandController.getInterpreter();
}

void MSXCommandController::signalMSXEvent(
	const Event& event, EmuTime /*time*/) noexcept
{
	if (getType(event) != EventType::MACHINE_ACTIVATED) return;

	// simple way to synchronize proxy settings
	for (const auto* s : settings) {
		try {
			getInterpreter().setVariable(
				s->getFullNameObj(), s->getValue());
		} catch (MSXException&) {
			// ignore
		}
	}
}

bool MSXCommandController::isActive() const
{
	return reactor.getMotherBoard() == &motherboard;
}

void MSXCommandController::transferSettings(const MSXCommandController& from)
{
	const auto& fromPrefix = from.getPrefix();
	const auto& manager = globalCommandController.getSettingsManager();
	for (const auto* s : settings) {
		if (const auto* fromSetting = manager.findSetting(fromPrefix, s->getBaseName())) {
			if (!fromSetting->needTransfer()) continue;
			try {
				getInterpreter().setVariable(
					s->getFullNameObj(), fromSetting->getValue());
			} catch (MSXException&) {
				// ignore
			}
		}
	}
}

} // namespace openmsx
