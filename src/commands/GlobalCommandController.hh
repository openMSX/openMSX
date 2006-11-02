// $Id$

#ifndef GLOBALCOMMANDCONTROLLER_HH
#define GLOBALCOMMANDCONTROLLER_HH

#include "CommandController.hh"
#include "noncopyable.hh"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace openmsx {

class EventDistributor;
class Reactor;
class CliComm;
class HotKey;
class InfoCommand;
class Interpreter;
class FileContext;
class HelpCmd;
class TabCompletionCmd;
class ProxyCmd;
class VersionInfo;
class RomInfoTopic;

class GlobalCommandController : public CommandController, private noncopyable
{
public:
	GlobalCommandController(EventDistributor& eventDistributor,
	                        Reactor& reactor);
	~GlobalCommandController();

	void setCliComm(CliComm* cliComm);

	InfoCommand& getOpenMSXInfoCommand();
	HotKey& getHotKey();

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
	 * Complete a given command
	 */
	void tabCompletion(std::string& command);

	void registerProxyCommand(const std::string& name);
	void unregisterProxyCommand(const std::string& name);

	void registerProxySetting(Setting& setting);
	void unregisterProxySetting(Setting& setting);

	// CommandController
	virtual void   registerCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void unregisterCompleter(CommandCompleter& completer,
	                                 const std::string& str);
	virtual void   registerCommand(Command& command,
	                               const std::string& str);
	virtual void unregisterCommand(Command& command,
	                               const std::string& str);
	virtual bool hasCommand(const std::string& command) const;
	virtual std::string executeCommand(const std::string& command,
	                                   CliConnection* connection = 0);
	virtual void splitList(const std::string& list,
	                       std::vector<std::string>& result);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual std::string makeUniqueSettingName(const std::string& name);
	virtual CliComm& getCliComm();
	virtual GlobalSettings& getGlobalSettings();
	virtual Interpreter& getInterpreter();
	virtual SettingsConfig& getSettingsConfig();
	virtual CliConnection* getConnection() const;
	virtual GlobalCommandController& getGlobalCommandController();

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
	Reactor& reactor;

	std::auto_ptr<Interpreter> interpreter;
	std::auto_ptr<InfoCommand> openMSXInfoCommand;
	std::auto_ptr<HotKey> hotKey;
	std::auto_ptr<SettingsConfig> settingsConfig;
	std::auto_ptr<GlobalSettings> globalSettings;

	friend class HelpCmd;
	const std::auto_ptr<HelpCmd> helpCmd;
	const std::auto_ptr<TabCompletionCmd> tabCompletionCmd;
	const std::auto_ptr<ProxyCmd> proxyCmd;
	const std::auto_ptr<VersionInfo> versionInfo;
	const std::auto_ptr<RomInfoTopic> romInfoTopic;

	std::map<std::string, unsigned> proxyCommandMap;
	std::map<std::string, unsigned> proxySettingMap;
};

} // namespace openmsx

#endif
