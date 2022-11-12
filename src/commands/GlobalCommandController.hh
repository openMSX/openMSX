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
#include "TclObject.hh"
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

#ifdef DEBUG
	hash_map<std::string, Command*,          XXHasher> commands;
#endif
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

	[[nodiscard]] InfoCommand& getOpenMSXInfoCommand() { return openMSXInfoCommand; }

	/**
	 * Executes all defined auto commands
	 */
	void source(const std::string& script);

	void registerProxyCommand(std::string_view name);
	void unregisterProxyCommand(std::string_view name);

	void registerProxySetting(Setting& setting);
	void unregisterProxySetting(Setting& setting);

	// CommandController
	void   registerCompleter(CommandCompleter& completer,
	                         std::string_view str) override;
	void unregisterCompleter(CommandCompleter& completer,
	                         std::string_view str) override;
	void   registerCommand(Command& command,
	                       zstring_view str) override;
	void unregisterCommand(Command& command,
	                       std::string_view str) override;
	TclObject executeCommand(zstring_view command,
	                         CliConnection* connection = nullptr) override;
	void registerSetting(Setting& setting) override;
	void unregisterSetting(Setting& setting) override;
	[[nodiscard]] CliComm& getCliComm() override;
	[[nodiscard]] Interpreter& getInterpreter() override;

	/**
	 * Complete the given command.
	 */
	[[nodiscard]] std::string tabCompletion(std::string_view command);

	/**
	 * Returns true iff the command is complete (all braces, quotes etc. are
	 * balanced).
	 */
	[[nodiscard]] bool isComplete(zstring_view command);

	[[nodiscard]] SettingsConfig& getSettingsConfig() { return settingsConfig; }
	[[nodiscard]] SettingsManager& getSettingsManager() { return settingsConfig.getSettingsManager(); }
	[[nodiscard]] CliConnection* getConnection() const { return connection; }

private:
	void tabCompletion(std::vector<std::string>& tokens);

	using ProxySettings =
		std::vector<std::pair<std::unique_ptr<ProxySetting>, unsigned>>;
	ProxySettings::iterator findProxySetting(std::string_view name);

	GlobalCliComm& cliComm;
	CliConnection* connection = nullptr;

	Reactor& reactor;

	Interpreter interpreter;
	InfoCommand openMSXInfoCommand;
	HotKey hotKey;
	SettingsConfig settingsConfig;

	struct HelpCmd final : Command {
		explicit HelpCmd(GlobalCommandController& controller);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} helpCmd;

	struct TabCompletionCmd final : Command {
		explicit TabCompletionCmd(GlobalCommandController& controller);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} tabCompletionCmd;

	struct UpdateCmd final : Command {
		explicit UpdateCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		CliConnection& getConnection();
	} updateCmd;

	struct PlatformInfo final : InfoTopic {
		explicit PlatformInfo(InfoCommand& openMSXInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} platformInfo;

	struct VersionInfo final : InfoTopic {
		explicit VersionInfo(InfoCommand& openMSXInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} versionInfo;

	RomInfoTopic romInfoTopic;

	struct NameFromProxy {
		template<typename Pair>
		[[nodiscard]] const std::string& operator()(const Pair& p) const {
			return p.second->getName();
		}
	};
	hash_set<std::pair<unsigned, std::unique_ptr<ProxyCmd>>, NameFromProxy, XXHasher> proxyCommandMap;
	ProxySettings proxySettings;
};

} // namespace openmsx

#endif
