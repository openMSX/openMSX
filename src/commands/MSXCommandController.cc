// $Id$

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
	, machineID(machineID_)
{
	globalCommandController.getInterpreter().createNamespace(machineID);

	machineInfoCommand.reset(new InfoCommand(*this, "machine_info"));
	machineInfoCommand->setAllowedInEmptyMachine(true);

	msxEventDistributor.registerEventListener(*this);
}

MSXCommandController::~MSXCommandController()
{
	msxEventDistributor.unregisterEventListener(*this);

	machineInfoCommand.reset();

	#ifndef NDEBUG
	for (CommandMap::const_iterator it = commandMap.begin();
	     it != commandMap.end(); ++it) {
		std::cout << "Command not unregistered: " << it->first() << std::endl;
	}
	for (SettingMap::const_iterator it = settingMap.begin();
	     it != settingMap.end(); ++it) {
		std::cout << "Setting not unregistered: " << it->first() << std::endl;
	}
	assert(commandMap.empty());
	assert(settingMap.empty());
	#endif

	globalCommandController.getInterpreter().deleteNamespace(machineID);
}

GlobalCommandController& MSXCommandController::getGlobalCommandController()
{
	return globalCommandController;
}

InfoCommand& MSXCommandController::getMachineInfoCommand()
{
	return *machineInfoCommand;
}

MSXMotherBoard& MSXCommandController::getMSXMotherBoard() const
{
	return motherboard;
}

string MSXCommandController::getFullName(string_ref name)
{
	return "::" + machineID + "::" + name;
}

void MSXCommandController::registerCommand(Command& command, const string& str)
{
	assert(!hasCommand(str));
	commandMap[str] = &command;

	string fullname = getFullName(str);
	globalCommandController.registerCommand(command, fullname);
	globalCommandController.registerProxyCommand(str);

	command.setAllowedInEmptyMachine(false);
}

void MSXCommandController::unregisterCommand(Command& command, string_ref str)
{
	assert(hasCommand(str));
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
	settingMap[name] = &setting;

	globalCommandController.registerProxySetting(setting);
	string fullname = getFullName(name);
	globalCommandController.getSettingsConfig().getSettingsManager()
		.registerSetting(setting, fullname);
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
	globalCommandController.getSettingsConfig().getSettingsManager()
		.unregisterSetting(setting, fullname);
}

void MSXCommandController::changeSetting(Setting& setting, const string& value)
{
	string fullname = getFullName(setting.getName());
	globalCommandController.changeSetting(fullname, value);
}

Command* MSXCommandController::findCommand(string_ref name) const
{
	CommandMap::const_iterator it = commandMap.find(name);
	return (it != commandMap.end()) ? it->second : nullptr;
}

Setting* MSXCommandController::findSetting(string_ref name)
{
	SettingMap::const_iterator it = settingMap.find(name);
	return (it != settingMap.end()) ? it->second : nullptr;
}

bool MSXCommandController::hasCommand(string_ref command) const
{
	return findCommand(command) != nullptr;
}

string MSXCommandController::executeCommand(const string& command,
                                            CliConnection* connection)
{
	return globalCommandController.executeCommand(command, connection);
}

void MSXCommandController::splitList(const string& list,
	                             vector<string>& result)
{
	globalCommandController.splitList(list, result);
}

CliComm& MSXCommandController::getCliComm()
{
	return motherboard.getMSXCliComm();
}

void MSXCommandController::signalEvent(
	const shared_ptr<const Event>& event, EmuTime::param /*time*/)
{
	if (event->getType() != OPENMSX_MACHINE_ACTIVATED) return;

	// simple way to synchronize proxy settings
	for (SettingMap::const_iterator it = settingMap.begin();
	     it != settingMap.end(); ++it) {
		changeSetting(*it->second, it->second->getValueString());
	}
}

bool MSXCommandController::isActive() const
{
	return reactor.getMotherBoard() == &motherboard;
}

} // namespace openmsx
