// $Id: $

#ifndef COMMANDREGISTRY_HH
#define COMMANDREGISTRY_HH

#include <string>

namespace openmsx {

class CommandCompleter;
class Command;
class Setting;

class CommandRegistry
{
public:
	/**
	 * (Un)register a command completer, used to complete build-in TCL cmds
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
	virtual bool hasCommand(const std::string& command) = 0;

	/** TODO
	 */
	virtual void   registerSetting(Setting& setting) = 0;
	virtual void unregisterSetting(Setting& setting) = 0;
	virtual std::string makeUniqueSettingName(const std::string& name) = 0;

protected:
	CommandRegistry() {}
	virtual ~CommandRegistry() {}
};

} // namespace openmsx

#endif
