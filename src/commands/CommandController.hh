#ifndef COMMANDCONTROLLER_HH
#define COMMANDCONTROLLER_HH

#include "TclObject.hh"
#include "string_ref.hh"

namespace openmsx {

class CommandCompleter;
class Command;
class CliConnection;
class Setting;
class BaseSetting;
class CliComm;
class Interpreter;

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
	virtual TclObject executeCommand(const std::string& command,
	                                 CliConnection* connection = nullptr) = 0;

	/** TODO
	 */
	virtual void   registerSetting(Setting& setting) = 0;
	virtual void unregisterSetting(Setting& setting) = 0;

	virtual CliComm& getCliComm() = 0;
	virtual Interpreter& getInterpreter() = 0;

protected:
	CommandController() {}
	~CommandController() {}
};

} // namespace openmsx

#endif
