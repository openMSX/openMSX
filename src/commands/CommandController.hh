#ifndef COMMANDCONTROLLER_HH
#define COMMANDCONTROLLER_HH

#include "string_ref.hh"
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
	                                 string_ref str) = 0;
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 string_ref str) = 0;

	/**
	 * (Un)register a command
	 */
	virtual void   registerCommand(Command& command,
	                               const std::string& str) = 0;
	virtual void unregisterCommand(Command& command,
	                               string_ref str) = 0;

	/**
	 * Does a command with this name already exist?
	 */
	virtual bool hasCommand(string_ref command) const = 0;

	/**
	 * Execute the given command
	 */
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = nullptr) = 0;

	virtual std::vector<std::string> splitList(const std::string& list) = 0;

	/** TODO
	 */
	virtual void   registerSetting(Setting& setting) = 0;
	virtual void unregisterSetting(Setting& setting) = 0;
	virtual Setting* findSetting(string_ref name) = 0;
	virtual void changeSetting(Setting& setting, const std::string& value) = 0;

	virtual CliComm& getCliComm() = 0;

protected:
	CommandController() {}
	virtual ~CommandController() {}
};

} // namespace openmsx

#endif
