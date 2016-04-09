#ifndef GLOBALCOMMANDCONTROLLER_HH
#define GLOBALCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "Command.hh"
#include "Interpreter.hh"
#include "InfoCommand.hh"
#include "InfoTopic.hh"
#include "HotKey.hh"
#include "SettingsConfig.hh"
#include "RomInfoTopic.hh"
#include "hash_map.hh"
#include "xxhash.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class EventDistributor;
class Reactor;
class GlobalCliComm;
class ProxyCmd;
class ProxySetting;

class GlobalCommandControllerBase
{
protected:
	~GlobalCommandControllerBase();

	hash_map<std::string, Command*,          XXHasher> commands;
	hash_map<std::string, CommandCompleter*, XXHasher> commandCompleters;
};

class GlobalCommandController final : private GlobalCommandControllerBase
                                    , public CommandController
{
public:
	GlobalCommandController(const GlobalCommandController&) = delete;
	GlobalCommandController& operator=(const GlobalCommandController&) = delete;

	GlobalCommandController(EventDistributor& eventDistributor,
	                        GlobalCliComm& cliComm,
	                        Reactor& reactor);
	~GlobalCommandController();

	InfoCommand& getOpenMSXInfoCommand() { return openMSXInfoCommand; }

	/**
	 * Executes all defined auto commands
	 */
	void source(const std::string& script);

	void registerProxyCommand(const std::string& name);
	void unregisterProxyCommand(string_ref name);

	void registerProxySetting(Setting& setting);
	void unregisterProxySetting(Setting& setting);

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

	SettingsConfig& getSettingsConfig() { return settingsConfig; }
	SettingsManager& getSettingsManager() { return settingsConfig.getSettingsManager(); }
	CliConnection* getConnection() const { return connection; }

private:
	void split(string_ref str,
	           std::vector<std::string>& tokens, char delimiter);
	std::string join(const std::vector<std::string>& tokens, char delimiter);
	std::string removeEscaping(const std::string& str);
	std::vector<std::string> removeEscaping(
		const std::vector<std::string>& input, bool keepLastIfEmpty);
	std::string addEscaping(const std::string& str, bool quote, bool finished);

	void tabCompletion(std::vector<std::string>& tokens);

	using ProxySettings =
		std::vector<std::pair<std::unique_ptr<ProxySetting>, unsigned>>;
	ProxySettings::iterator findProxySetting(string_ref name);

	GlobalCliComm& cliComm;
	CliConnection* connection;

	Reactor& reactor;

	Interpreter interpreter;
	InfoCommand openMSXInfoCommand;
	HotKey hotKey;
	SettingsConfig settingsConfig;

	struct HelpCmd final : Command {
		explicit HelpCmd(GlobalCommandController& controller);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} helpCmd;

	struct TabCompletionCmd final : Command {
		explicit TabCompletionCmd(GlobalCommandController& controller);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} tabCompletionCmd;

	struct UpdateCmd final : Command {
		explicit UpdateCmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		CliConnection& getConnection();
	} updateCmd;

	struct PlatformInfo final : InfoTopic {
		explicit PlatformInfo(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} platformInfo;

	struct VersionInfo final : InfoTopic {
		explicit VersionInfo(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} versionInfo;

	RomInfoTopic romInfoTopic;

	struct NameFromProxy {
		template<typename Pair>
		const std::string& operator()(const Pair& p) const {
			return p.second->getName();
		}
	};
	hash_set<std::pair<unsigned, std::unique_ptr<ProxyCmd>>, NameFromProxy, XXHasher> proxyCommandMap;
	ProxySettings proxySettings;
};

} // namespace openmsx

#endif
