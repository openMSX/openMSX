// $Id$

#ifndef COMMANDCONTROLLER_HH
#define COMMANDCONTROLLER_HH

#include <string>
#include <vector>

namespace openmsx {

class CommandCompleter;
class Command;
class CliConnection;
class Setting;
class CliComm;
class Interpreter;
class SettingsConfig;
class Reactor;

class CommandController
{
public:
	/**
	 * (Un)register a command completer, used to complete build-in Tcl cmds
	 */
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 const std::string& str) = 0;
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 const std::string& str) = 0;

	/**
	 * (Un)register a command
	 */
	virtual void   registerCommand(Command& command,
	                               const std::string& str) = 0;
	virtual void unregisterCommand(Command& command,
	                               const std::string& str) = 0;

	/**
	 * Does a command with this name already exist?
	 */
	virtual bool hasCommand(const std::string& command) const = 0;

	/**
	 * Execute the given command
	 */
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = 0) = 0;

	/**
	 * Complete a given command
	 */
	virtual void tabCompletion(std::string& command) = 0;

	/**
	 * Returns true iff the command is complete
	 * (all braces, quotes, .. are balanced)
	 */
	virtual bool isComplete(const std::string& command) = 0;

	virtual void splitList(const std::string& list,
	                       std::vector<std::string>& result) = 0;

	/** TODO
	 */
	virtual void   registerSetting(Setting& setting) = 0;
	virtual void unregisterSetting(Setting& setting) = 0;
	virtual Setting* findSetting(const std::string& name) = 0;
	virtual void changeSetting(Setting& setting, const std::string& value) = 0;
	virtual std::string makeUniqueSettingName(const std::string& name) = 0;

	virtual CliComm& getCliComm() = 0;
	virtual Interpreter& getInterpreter() = 0;
	virtual SettingsConfig& getSettingsConfig() = 0;
	virtual CliConnection* getConnection() const = 0;

protected:
	CommandController() {}
	virtual ~CommandController() {}
};

} // namespace openmsx

#endif
