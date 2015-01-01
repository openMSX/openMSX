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
#include "StringMap.hh"
#include "noncopyable.hh"
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

	StringMap<Command*> commands;
	StringMap<CommandCompleter*> commandCompleters;
};

class GlobalCommandController final : private GlobalCommandControllerBase
                                    , public CommandController, private noncopyable
{
public:
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

	SettingsConfig& getSettingsConfig() { return settingsConfig; }
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

	typedef std::vector<std::pair<std::unique_ptr<ProxySetting>, unsigned>>
		ProxySettings;
	ProxySettings::iterator findProxySetting(const std::string& name);

	GlobalCliComm& cliComm;
	CliConnection* connection;

	Reactor& reactor;

	Interpreter interpreter;
	InfoCommand openMSXInfoCommand;
	HotKey hotKey;
	SettingsConfig settingsConfig;

	class HelpCmd final : public Command {
	public:
		explicit HelpCmd(GlobalCommandController& controller);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		GlobalCommandController& controller;
	} helpCmd;

	class TabCompletionCmd final : public Command {
	public:
		explicit TabCompletionCmd(GlobalCommandController& controller);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	private:
		GlobalCommandController& controller;
	} tabCompletionCmd;

	class UpdateCmd final : public Command {
	public:
		explicit UpdateCmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		CliConnection& getConnection();
	};
	std::unique_ptr<UpdateCmd> updateCmd;

	class PlatformInfo final : public InfoTopic {
	public:
		explicit PlatformInfo(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} platformInfo;

	class VersionInfo final : public InfoTopic {
	public:
		explicit VersionInfo(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} versionInfo;

	RomInfoTopic romInfoTopic;

	StringMap<std::pair<unsigned, std::unique_ptr<ProxyCmd>>> proxyCommandMap;
	ProxySettings proxySettings;
};

} // namespace openmsx

#endif
