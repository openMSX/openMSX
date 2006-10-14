// $Id: $

#include "MSXCommandController.hh"
#include "GlobalCommandController.hh"
#include "InfoCommand.hh"

using std::string;
using std::vector;

namespace openmsx {

MSXCommandController::MSXCommandController(
	GlobalCommandController& globalCommandController_)
	: globalCommandController(globalCommandController_)
	, machineInfoCommand(new InfoCommand(*this, "machine_info"))
{
}

MSXCommandController::~MSXCommandController()
{
	//assert(commands.empty());            // TODO
	//assert(commandCompleters.empty());   // TODO
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
	//assert(commands.find(str) == commands.end());
	//commands[str] = &command;
	globalCommandController.registerCommand(command, str);
}

void MSXCommandController::unregisterCommand(Command& command, const string& str)
{
	//assert(commands.find(str) != commands.end());
	//assert(commands.find(str)->second == &command);
	globalCommandController.unregisterCommand(command, str);
	//commands.erase(str);
}

void MSXCommandController::registerCompleter(CommandCompleter& completer,
                                             const string& str)
{
	//assert(commandCompleters.find(str) == commandCompleters.end());
	//commandCompleters[str] = &completer;
	globalCommandController.registerCompleter(completer, str);
}

void MSXCommandController::unregisterCompleter(CommandCompleter& completer,
                                               const string& str)
{
	//(void)completer;
	//assert(commandCompleters.find(str) != commandCompleters.end());
	//assert(commandCompleters.find(str)->second == &completer);
	//commandCompleters.erase(str);
	globalCommandController.unregisterCompleter(completer, str);
}

void MSXCommandController::registerSetting(Setting& setting)
{
	//getSettingsConfig().getSettingsManager().registerSetting(setting);
	globalCommandController.registerSetting(setting);
}

void MSXCommandController::unregisterSetting(Setting& setting)
{
	//getSettingsConfig().getSettingsManager().unregisterSetting(setting);
	globalCommandController.unregisterSetting(setting);
}

string MSXCommandController::makeUniqueSettingName(const string& name)
{
	//return getSettingsConfig().getSettingsManager().makeUnique(name);
	return globalCommandController.makeUniqueSettingName(name);
}

bool MSXCommandController::hasCommand(const string& command)
{
	//return commands.find(command) != commands.end();
	return globalCommandController.hasCommand(command);
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
