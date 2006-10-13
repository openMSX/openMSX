// $Id$

#ifndef COMMANDCONTROLLER_HH
#define COMMANDCONTROLLER_HH

#include "CommandRegistry.hh"
#include "noncopyable.hh"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace openmsx {

class EventDistributor;
class CliComm;
class CliConnection;
class HotKey;
class InfoCommand;
class Interpreter;
class FileContext;
class SettingsConfig;
class GlobalSettings;
class HelpCmd;
class TabCompletionCmd;
class VersionInfo;
class RomInfoTopic;

class CommandController : public CommandRegistry, private noncopyable
{
public:
	explicit CommandController(EventDistributor& eventDistributor);
	~CommandController();

	void setCliComm(CliComm* cliComm);

	CliComm& getCliComm();
	Interpreter& getInterpreter();
	InfoCommand& getOpenMSXInfoCommand();
	InfoCommand& getMachineInfoCommand();
	HotKey& getHotKey();
	SettingsConfig& getSettingsConfig();
	GlobalSettings& getGlobalSettings();

	CliConnection* getConnection() const;

	/**
	 * Executes all defined auto commands
	 */
	void source(const std::string& script);

	/**
	 * Returns true iff the command is complete
	 * (all braces, quotes, .. are balanced)
	 */
	bool isComplete(const std::string& command);

	/**
	 * Execute a given command
	 */
	std::string executeCommand(const std::string& command,
	                           CliConnection* connection = NULL);

	/**
	 * Complete a given command
	 */
	void tabCompletion(std::string& command);

	// CommandRegistry
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void   registerCommand(Command& command,
	                               const std::string& str);
	virtual void unregisterCommand(Command& command,
	                               const std::string& str);
	virtual bool hasCommand(const std::string& command);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual std::string makeUniqueSettingName(const std::string& name);
	virtual CommandController& getCommandController();

private:
	void split(const std::string& str,
	           std::vector<std::string>& tokens, char delimiter);
	std::string join(const std::vector<std::string>& tokens, char delimiter);
	std::string removeEscaping(const std::string& str);
	void removeEscaping(const std::vector<std::string>& input,
	                    std::vector<std::string>& result, bool keepLastIfEmpty);
	std::string addEscaping(const std::string& str, bool quote, bool finished);

	void tabCompletion(std::vector<std::string>& tokens);

	typedef std::map<std::string, Command*> CommandMap;
	typedef std::map<std::string, CommandCompleter*> CompleterMap;
	CommandMap commands;
	CompleterMap commandCompleters;

	CliComm* cliComm;
	CliConnection* connection;

	EventDistributor& eventDistributor;

	std::auto_ptr<Interpreter> interpreter;
	std::auto_ptr<InfoCommand> openMSXInfoCommand;
	std::auto_ptr<InfoCommand> machineInfoCommand;
	std::auto_ptr<HotKey> hotKey;
	std::auto_ptr<SettingsConfig> settingsConfig;
	std::auto_ptr<GlobalSettings> globalSettings;

	friend class HelpCmd;
	const std::auto_ptr<HelpCmd> helpCmd;
	const std::auto_ptr<TabCompletionCmd> tabCompletionCmd;
	const std::auto_ptr<VersionInfo> versionInfo;
	const std::auto_ptr<RomInfoTopic> romInfoTopic;
};

} // namespace openmsx

#endif
