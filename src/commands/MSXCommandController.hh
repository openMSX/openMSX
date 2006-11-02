// $Id: $

#ifndef MSXCOMMANDCONTROLLER_HH
#define MSXCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "noncopyable.hh"
#include <map>
#include <memory>

namespace openmsx {

class GlobalCommandController;
class InfoCommand;

class MSXCommandController : public CommandController, private noncopyable
{
public:
	explicit MSXCommandController(
	                     GlobalCommandController& globalCommandController);
	~MSXCommandController();

	InfoCommand& getMachineInfoCommand();

	Command* findCommand(const std::string& name) const;
	Setting* findSetting(const std::string& name) const;

	// CommandController
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void   registerCommand(Command& command,
	                               const std::string& str);
	virtual void unregisterCommand(Command& command,
	                               const std::string& str);
	virtual bool hasCommand(const std::string& command) const;
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = 0);
	virtual void splitList(const std::string& list,
	                       std::vector<std::string>& result);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual std::string makeUniqueSettingName(const std::string& name);
	virtual CliComm& getCliComm();
	virtual GlobalSettings& getGlobalSettings();
	virtual Interpreter& getInterpreter();
	virtual SettingsConfig& getSettingsConfig();
	virtual CliConnection* getConnection() const;
	virtual GlobalCommandController& getGlobalCommandController();

private:
	const std::string& getNamespace();

	GlobalCommandController& globalCommandController;
	std::auto_ptr<InfoCommand> machineInfoCommand;

	std::string namespace_;

	typedef std::map<std::string, Command*> CommandMap;
	CommandMap commandMap;
	typedef std::map<std::string, Setting*> SettingMap;
	SettingMap settingMap;
};

} // namespace openmsx

#endif
