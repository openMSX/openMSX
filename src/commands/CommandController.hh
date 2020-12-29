#ifndef COMMANDCONTROLLER_HH
#define COMMANDCONTROLLER_HH

#include "TclObject.hh"
#include "zstring_view.hh"
#include <string_view>

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
	                                 std::string_view str) = 0;
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 std::string_view str) = 0;

	/**
	 * (Un)register a command
	 */
	virtual void   registerCommand(Command& command,
	                               zstring_view str) = 0;
	virtual void unregisterCommand(Command& command,
	                               std::string_view str) = 0;

	/**
	 * Execute the given command
	 */
	virtual TclObject executeCommand(zstring_view command,
	                                 CliConnection* connection = nullptr) = 0;

	/** TODO
	 */
	virtual void   registerSetting(Setting& setting) = 0;
	virtual void unregisterSetting(Setting& setting) = 0;

	virtual CliComm& getCliComm() = 0;
	virtual Interpreter& getInterpreter() = 0;

protected:
	CommandController() = default;
	~CommandController() = default;
};

} // namespace openmsx

#endif
