#include "MSXCommandController.hh"
#include "GlobalCommandController.hh"
#include "Reactor.hh"
#include "MSXEventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "SettingsConfig.hh"
#include "SettingsManager.hh"
#include "InfoCommand.hh"
#include "Interpreter.hh"
#include "Setting.hh"
#include "Event.hh"
#include "MSXException.hh"
#include "KeyRange.hh"
#include "memory.hh"
#include <iostream>

using std::string;
using std::vector;

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
	, machineID("::" + machineID_ + "::")
{
	globalCommandController.getInterpreter().createNamespace(machineID);

	machineInfoCommand = make_unique<InfoCommand>(*this, "machine_info");
	machineInfoCommand->setAllowedInEmptyMachine(true);

	msxEventDistributor.registerEventListener(*this);
}

MSXCommandController::~MSXCommandController()
{
	msxEventDistributor.unregisterEventListener(*this);

	machineInfoCommand.reset();

	#ifndef NDEBUG
	for (auto* c : commandMap) {
		std::cout << "Command not unregistered: " << c->getName() << std::endl;
	}
	for (auto* s : settingMap) {
		std::cout << "Setting not unregistered: " << s->getName() << std::endl;
	}
	assert(commandMap.empty());
	assert(settingMap.empty());
	#endif

	globalCommandController.getInterpreter().deleteNamespace(machineID);
}

string MSXCommandController::getFullName(string_ref name)
{
	return machineID + name;
}

void MSXCommandController::registerCommand(Command& command, const string& str)
{
	assert(!hasCommand(str));
	assert(command.getName() == str);
	commandMap.insert_noDuplicateCheck(&command);

	string fullname = getFullName(str);
	globalCommandController.registerCommand(command, fullname);
	globalCommandController.registerProxyCommand(str);

	command.setAllowedInEmptyMachine(false);
}

void MSXCommandController::unregisterCommand(Command& command, string_ref str)
{
	assert(hasCommand(str));
	assert(command.getName() == str);
	commandMap.erase(str);

	globalCommandController.unregisterProxyCommand(str);
	string fullname = getFullName(str);
	globalCommandController.unregisterCommand(command, fullname);
}

void MSXCommandController::registerCompleter(CommandCompleter& completer,
                                             string_ref str)
{
	string fullname = getFullName(str);
	globalCommandController.registerCompleter(completer, fullname);
}

void MSXCommandController::unregisterCompleter(CommandCompleter& completer,
                                               string_ref str)
{
	string fullname = getFullName(str);
	globalCommandController.unregisterCompleter(completer, fullname);
}

void MSXCommandController::registerSetting(Setting& setting)
{
	const string& name = setting.getName();
	assert(!findSetting(name));
	settingMap.insert_noDuplicateCheck(&setting);

	globalCommandController.registerProxySetting(setting);
	string fullname = getFullName(name);
	globalCommandController.getSettingsManager().registerSetting(setting, fullname);
	globalCommandController.getInterpreter().registerSetting(setting, fullname);
}

void MSXCommandController::unregisterSetting(Setting& setting)
{
	const string& name = setting.getName();
	assert(findSetting(name));
	settingMap.erase(name);

	globalCommandController.unregisterProxySetting(setting);
	string fullname = getFullName(name);
	globalCommandController.getInterpreter().unregisterSetting(setting, fullname);
	globalCommandController.getSettingsManager().unregisterSetting(setting, fullname);
}

void MSXCommandController::changeSetting(Setting& setting, const TclObject& value)
{
	string fullname = getFullName(setting.getName());
	globalCommandController.changeSetting(fullname, value);
}

Command* MSXCommandController::findCommand(string_ref name) const
{
	auto it = commandMap.find(name);
	return (it != end(commandMap)) ? *it : nullptr;
}

BaseSetting* MSXCommandController::findSetting(string_ref name)
{
	auto it = settingMap.find(name);
	return (it != end(settingMap)) ? *it : nullptr;
}

const BaseSetting* MSXCommandController::findSetting(string_ref setting) const
{
	return const_cast<MSXCommandController*>(this)->findSetting(setting);
}

bool MSXCommandController::hasCommand(string_ref command) const
{
	return findCommand(command) != nullptr;
}

TclObject MSXCommandController::executeCommand(const string& command,
                                               CliConnection* connection)
{
	return globalCommandController.executeCommand(command, connection);
}

CliComm& MSXCommandController::getCliComm()
{
	return motherboard.getMSXCliComm();
}

Interpreter& MSXCommandController::getInterpreter()
{
	return globalCommandController.getInterpreter();
}

void MSXCommandController::signalEvent(
	const std::shared_ptr<const Event>& event, EmuTime::param /*time*/)
{
	if (event->getType() != OPENMSX_MACHINE_ACTIVATED) return;

	// simple way to synchronize proxy settings
	for (auto* s : settingMap) {
		try {
			changeSetting(*s, s->getValue());
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
	for (auto* s : settingMap) {
		if (auto* fromSetting = from.findSetting(s->getName())) {
			if (!fromSetting->needTransfer()) continue;
			try {
				changeSetting(*s, fromSetting->getValue());
			} catch (MSXException&) {
				// ignore
			}
		}
	}
}

} // namespace openmsx
