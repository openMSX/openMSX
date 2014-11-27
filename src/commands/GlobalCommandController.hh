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
class PlatformInfo;
class VersionInfo;
class RomInfoTopic;
class ProxySetting;
class SettingsConfig;

class GlobalCommandController final : public CommandController, private noncopyable
{
public:
	GlobalCommandController(EventDistributor& eventDistributor,
	                        GlobalCliComm& cliComm,
	                        Reactor& reactor);
	~GlobalCommandController();

	InfoCommand& getOpenMSXInfoCommand() { return *openMSXInfoCommand; }

	/**
	 * Executes all defined auto commands
	 */
	void source(const std::string& script);

	void registerProxyCommand(const std::string& name);
	void unregisterProxyCommand(string_ref name);

	void registerProxySetting(Setting& setting);
	void unregisterProxySetting(Setting& setting);

	void changeSetting(const std::string& name, const TclObject& value);

	// CommandController
	void   registerCompleter(CommandCompleter& completer,
	                         string_ref str) override;
	void unregisterCompleter(CommandCompleter& completer,
	                         string_ref str) override;
	void   registerCommand(Command& command,
	                       const std::string& str) override;
	void unregisterCommand(Command& command,
	                       string_ref str) override;
	bool hasCommand(string_ref command) const override;
	TclObject executeCommand(const std::string& command,
	                         CliConnection* connection = nullptr) override;
	void registerSetting(Setting& setting) override;
	void unregisterSetting(Setting& setting) override;
	BaseSetting* findSetting(string_ref name) override;
	void changeSetting(Setting& setting, const TclObject& value) override;
	CliComm& getCliComm() override;
	Interpreter& getInterpreter() override;

	/**
	 * Complete the given command.
	 */
	std::string tabCompletion(string_ref command);

	/**
	 * Returns true iff the command is complete (all braces, quotes etc. are
	 * balanced).
	 */
	bool isComplete(const std::string& command);

	SettingsConfig& getSettingsConfig();
	CliConnection* getConnection() const;

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
	std::unique_ptr<PlatformInfo> platformInfo;
	std::unique_ptr<VersionInfo> versionInfo;
	std::unique_ptr<RomInfoTopic> romInfoTopic;

	StringMap<std::pair<unsigned, std::unique_ptr<ProxyCmd>>> proxyCommandMap;
	ProxySettings proxySettings;
};

} // namespace openmsx

#endif
