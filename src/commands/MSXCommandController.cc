// $Id: $

#include "MSXCommandController.hh"
#include "GlobalCommandController.hh"
#include "SettingsConfig.hh"
#include "SettingsManager.hh"
#include "InfoCommand.hh"
#include "Interpreter.hh"
#include "Setting.hh"
#include "StringOp.hh"

using std::string;
using std::vector;

namespace openmsx {

MSXCommandController::MSXCommandController(
	GlobalCommandController& globalCommandController_)
	: globalCommandController(globalCommandController_)
{
	getInterpreter().createNamespace(getNamespace());

	machineInfoCommand.reset(new InfoCommand(*this, "machine_info"));
}

MSXCommandController::~MSXCommandController()
{
	machineInfoCommand.reset();

	assert(commandMap.empty());
	assert(settingMap.empty());

	getInterpreter().deleteNamespace(getNamespace());
}

const std::string& MSXCommandController::getNamespace()
{
	if (namespace_.empty()) {
		static unsigned counter = 0;
		namespace_ = "machine" + StringOp::toString(++counter);
	}
	return namespace_;
}

InfoCommand& MSXCommandController::getMachineInfoCommand()
{
	return *machineInfoCommand;
}

GlobalCommandController& MSXCommandController::getGlobalCommandController()
{
	return globalCommandController;
}

void MSXCommandController::registerCommand(Command& command, const string& str)
{
	assert(!hasCommand(str));
	commandMap[str] = &command;

	string fullname = getNamespace() + "::" + str;
	globalCommandController.registerCommand(command, fullname);
	globalCommandController.registerProxyCommand(str);
}

void MSXCommandController::unregisterCommand(Command& command, const string& str)
{
	assert(hasCommand(str));
	commandMap.erase(str);

	globalCommandController.unregisterProxyCommand(str);
	string fullname = getNamespace() + "::" + str;
	globalCommandController.unregisterCommand(command, fullname);
}

void MSXCommandController::registerCompleter(CommandCompleter& completer,
                                             const string& str)
{
	string fullname = getNamespace() + "::" + str;
	globalCommandController.registerCompleter(completer, fullname);
}

void MSXCommandController::unregisterCompleter(CommandCompleter& completer,
                                               const string& str)
{
	string fullname = getNamespace() + "::" + str;
	globalCommandController.unregisterCompleter(completer, fullname);
}

void MSXCommandController::registerSetting(Setting& setting)
{
	string name = setting.getName();
	assert(!findSetting(name));
	settingMap[name] = &setting;

	globalCommandController.registerProxySetting(setting);
	string fullname = getNamespace() + "::" + name;
	getSettingsConfig().getSettingsManager().registerSetting(setting, fullname);
	getInterpreter().registerSetting(setting, fullname);
}

void MSXCommandController::unregisterSetting(Setting& setting)
{
	string name = setting.getName();
	assert(findSetting(name));
	settingMap.erase(name);

	globalCommandController.unregisterProxySetting(setting);
	string fullname = getNamespace() + "::" + name;
	getInterpreter().unregisterSetting(setting, fullname);
	getSettingsConfig().getSettingsManager().unregisterSetting(setting, fullname);
}

string MSXCommandController::makeUniqueSettingName(const string& name)
{
	//return getSettingsConfig().getSettingsManager().makeUnique(name);
	return globalCommandController.makeUniqueSettingName(name);
}

Command* MSXCommandController::findCommand(const std::string& name) const
{
	CommandMap::const_iterator it = commandMap.find(name);
	return (it != commandMap.end()) ? it->second : NULL;
}

Setting* MSXCommandController::findSetting(const std::string& name) const
{
	SettingMap::const_iterator it = settingMap.find(name);
	return (it != settingMap.end()) ? it->second : NULL;
}

bool MSXCommandController::hasCommand(const string& command) const
{
	return findCommand(command);
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
	return globalCommandController.getCliComm();
}

GlobalSettings& MSXCommandController::getGlobalSettings()
{
	return globalCommandController.getGlobalSettings();
}

Interpreter& MSXCommandController::getInterpreter()
{
	return globalCommandController.getInterpreter();
}

SettingsConfig& MSXCommandController::getSettingsConfig()
{
	return globalCommandController.getSettingsConfig();
}

CliConnection* MSXCommandController::getConnection() const
{
	return globalCommandController.getConnection();
}

} // namespace openmsx
