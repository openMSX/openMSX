// $Id$

#ifndef MSXCOMMANDCONTROLLER_HH
#define MSXCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "MSXEventListener.hh"
#include "noncopyable.hh"
#include <map>
#include <memory>

namespace openmsx {

class GlobalCommandController;
class MSXMotherBoard;
class InfoCommand;

class MSXCommandController : public CommandController, private MSXEventListener,
                             private noncopyable
{
public:
	MSXCommandController(GlobalCommandController& globalCommandController,
	                     MSXMotherBoard& motherboard);
	~MSXCommandController();

	GlobalCommandController& getGlobalCommandController();
	InfoCommand& getMachineInfoCommand();

	Command* findCommand(const std::string& name) const;

	void activated();

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
	virtual void tabCompletion(std::string& command);
	virtual bool isComplete(const std::string& command);
	virtual void splitList(const std::string& list,
	                       std::vector<std::string>& result);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual Setting* findSetting(const std::string& name);
	virtual void changeSetting(Setting& setting, const std::string& value);
	virtual std::string makeUniqueSettingName(const std::string& name);
	virtual CliComm& getCliComm();
	virtual CliComm* getCliCommIfAvailable();
	virtual GlobalSettings& getGlobalSettings();
	virtual Interpreter& getInterpreter();
	virtual SettingsConfig& getSettingsConfig();
	virtual CliConnection* getConnection() const;
	virtual Reactor& getReactor() const;

private:
	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	GlobalCommandController& globalCommandController;
	MSXMotherBoard& motherboard;
	std::auto_ptr<InfoCommand> machineInfoCommand;

	typedef std::map<std::string, Command*> CommandMap;
	CommandMap commandMap;
	typedef std::map<std::string, Setting*> SettingMap;
	SettingMap settingMap;
};

} // namespace openmsx

#endif
