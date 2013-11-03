#ifndef GLOBALCOMMANDCONTROLLER_HH
#define GLOBALCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "StringMap.hh"
#include "noncopyable.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class EventDistributor;
class Reactor;
class GlobalCliComm;
class HotKey;
class InfoCommand;
class Interpreter;
class HelpCmd;
class TabCompletionCmd;
class UpdateCmd;
class ProxyCmd;
class VersionInfo;
class RomInfoTopic;
class ProxySetting;
class SettingsConfig;

class GlobalCommandController : public CommandController, private noncopyable
{
public:
	GlobalCommandController(EventDistributor& eventDistributor,
	                        GlobalCliComm& cliComm,
	                        Reactor& reactor);
	~GlobalCommandController();

	InfoCommand& getOpenMSXInfoCommand();

	/**
	 * Executes all defined auto commands
	 */
	void source(const std::string& script);

	void registerProxyCommand(const std::string& name);
	void unregisterProxyCommand(string_ref name);

	void registerProxySetting(Setting& setting);
	void unregisterProxySetting(Setting& setting);

	void changeSetting(const std::string& name, const std::string& value);

	// CommandController
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 string_ref str);
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 string_ref str);
	virtual void   registerCommand(Command& command,
	                               const std::string& str);
	virtual void unregisterCommand(Command& command,
	                               string_ref str);
	virtual bool hasCommand(string_ref command) const;
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = nullptr);
	/**
	 * Complete the given command.
	 */
	virtual std::string tabCompletion(string_ref command);
	/**
	 * Returns true iff the command is complete (all braces, quotes etc. are
	 * balanced).
	 */
	virtual bool isComplete(const std::string& command);
	virtual std::vector<std::string> splitList(const std::string& list);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual BaseSetting* findSetting(string_ref name);
	virtual void changeSetting(Setting& setting, const std::string& value);
	virtual CliComm& getCliComm();
	virtual Interpreter& getInterpreter();
	virtual SettingsConfig& getSettingsConfig();
	virtual CliConnection* getConnection() const;

private:
	void split(string_ref str,
	           std::vector<std::string>& tokens, char delimiter);
	std::string join(const std::vector<std::string>& tokens, char delimiter);
	std::string removeEscaping(const std::string& str);
	std::vector<std::string> removeEscaping(
		const std::vector<std::string>& input, bool keepLastIfEmpty);
	std::string addEscaping(const std::string& str, bool quote, bool finished);

	void tabCompletion(std::vector<std::string>& tokens);

	typedef std::vector<std::pair<std::unique_ptr<ProxySetting>, unsigned>>
		ProxySettings;
	ProxySettings::iterator findProxySetting(const std::string& name);

	StringMap<Command*> commands;
	StringMap<CommandCompleter*> commandCompleters;

	GlobalCliComm& cliComm;
	CliConnection* connection;

	Reactor& reactor;

	std::unique_ptr<Interpreter> interpreter;
	std::unique_ptr<InfoCommand> openMSXInfoCommand;
	std::unique_ptr<HotKey> hotKey;
	std::unique_ptr<SettingsConfig> settingsConfig;

	friend class HelpCmd;
	std::unique_ptr<HelpCmd> helpCmd;
	std::unique_ptr<TabCompletionCmd> tabCompletionCmd;
	std::unique_ptr<UpdateCmd> updateCmd;
	std::unique_ptr<ProxyCmd> proxyCmd;
	std::unique_ptr<VersionInfo> versionInfo;
	std::unique_ptr<RomInfoTopic> romInfoTopic;

	StringMap<unsigned> proxyCommandMap;
	ProxySettings proxySettings;
};

} // namespace openmsx

#endif
