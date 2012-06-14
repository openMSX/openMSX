// $Id$

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
class FileContext;
class HelpCmd;
class TabCompletionCmd;
class UpdateCmd;
class ProxyCmd;
class VersionInfo;
class RomInfoTopic;
class ProxySetting;

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
	                                   CliConnection* connection = 0);
	/**
	 * Complete the given command.
	 */
	virtual void tabCompletion(std::string& command);
	/**
	 * Returns true iff the command is complete (all braces, quotes etc. are
	 * balanced).
	 */
	virtual bool isComplete(const std::string& command);
	virtual void splitList(const std::string& list,
	                       std::vector<std::string>& result);
	virtual void registerSetting(Setting& setting);
	virtual void unregisterSetting(Setting& setting);
	virtual Setting* findSetting(string_ref name);
	virtual void changeSetting(Setting& setting, const std::string& value);
	virtual CliComm& getCliComm();
	virtual Interpreter& getInterpreter();
	virtual SettingsConfig& getSettingsConfig();
	virtual CliConnection* getConnection() const;

private:
	void split(const std::string& str,
	           std::vector<std::string>& tokens, char delimiter);
	std::string join(const std::vector<std::string>& tokens, char delimiter);
	std::string removeEscaping(const std::string& str);
	void removeEscaping(const std::vector<std::string>& input,
	                    std::vector<std::string>& result, bool keepLastIfEmpty);
	std::string addEscaping(const std::string& str, bool quote, bool finished);

	void tabCompletion(std::vector<std::string>& tokens);

	typedef std::vector<std::pair<ProxySetting*, unsigned> > ProxySettings;
	ProxySettings::iterator findProxySetting(const std::string& name);

	typedef StringMap<Command*> CommandMap;
	typedef StringMap<CommandCompleter*> CompleterMap;
	CommandMap commands;
	CompleterMap commandCompleters;

	GlobalCliComm& cliComm;
	CliConnection* connection;

	Reactor& reactor;

	std::auto_ptr<Interpreter> interpreter;
	std::auto_ptr<InfoCommand> openMSXInfoCommand;
	std::auto_ptr<HotKey> hotKey;
	std::auto_ptr<SettingsConfig> settingsConfig;

	friend class HelpCmd;
	std::auto_ptr<HelpCmd> helpCmd;
	std::auto_ptr<TabCompletionCmd> tabCompletionCmd;
	std::auto_ptr<UpdateCmd> updateCmd;
	std::auto_ptr<ProxyCmd> proxyCmd;
	std::auto_ptr<VersionInfo> versionInfo;
	std::auto_ptr<RomInfoTopic> romInfoTopic;

	StringMap<unsigned> proxyCommandMap;
	ProxySettings proxySettings;
};

} // namespace openmsx

#endif
