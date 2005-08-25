// $Id$

#ifndef COMMANDCONTROLLER_HH
#define COMMANDCONTROLLER_HH

#include "Command.hh"
#include <string>
#include <map>
#include <set>
#include <vector>
#include <memory>

namespace openmsx {

class CommandConsole;
class InfoCommand;
class Interpreter;
class FileContext;

class CommandController
{
public:
	static CommandController& instance();

	/** Returns the TCL interpreter that is associated with these commands
	 */
	Interpreter& getInterpreter();

	/**
	 * (Un)register a command
	 */
	void registerCommand(Command* command, const std::string& str);
	void unregisterCommand(Command* command, const std::string& str);

	/**
	 * (Un)register a command completer, used to complete build-in TCL cmds
	 */
	void registerCompleter(CommandCompleter* completer, const std::string& str);
	void unregisterCompleter(CommandCompleter* completer, const std::string& str);

	/**
	 * Does a command with this name already exist?
	 */
	bool hasCommand(const std::string& command);

	/**
	 * Executes all defined auto commands
	 */
	void autoCommands();

	/**
	 * Returns true iff the command is complete
	 * (all braces, quotes, .. are balanced)
	 */
	bool isComplete(const std::string& command) const;

	/**
	 * Execute a given command
	 */
	std::string executeCommand(const std::string& command);

	/**
	 * Complete a given command
	 */
	void tabCompletion(std::string& command);

	/**
	 * TODO
	 */
	static void completeString(std::vector<std::string>& tokens,
	                           std::set<std::string>& set,
	                           bool caseSensitive = true);
	static void completeFileName(std::vector<std::string>& tokens);
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context);
	static void completeFileName(std::vector<std::string>& tokens,
                                     const FileContext& context,
                                     const std::set<std::string>& extra);

	// should only be called by CommandConsole
	void setCommandConsole(CommandConsole* console);

private:
	CommandController();
	~CommandController();

	void split(const std::string& str,
	           std::vector<std::string>& tokens, char delimiter);
	std::string join(const std::vector<std::string>& tokens, char delimiter);
	std::string removeEscaping(const std::string& str);
	void removeEscaping(const std::vector<std::string>& input,
	                    std::vector<std::string>& result, bool keepLastIfEmpty);
	std::string addEscaping(const std::string& str, bool quote, bool finished);

	void tabCompletion(std::vector<std::string>& tokens);
	static bool completeString2(std::string& str, std::set<std::string>& set,
	                            bool caseSensitive);
	static bool equal(const std::string& s1, const std::string& s2,
	                  bool caseSensitive);

	typedef std::map<std::string, Command*> CommandMap;
	typedef std::map<std::string, CommandCompleter*> CompleterMap;
	CommandMap commands;
	CompleterMap commandCompleters;

	// Commands
	class HelpCmd : public SimpleCommand {
	public:
		HelpCmd(CommandController& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		CommandController& parent;
	} helpCmd;

	CommandConsole* cmdConsole;

	InfoCommand& infoCommand;
	const std::auto_ptr<Interpreter> interpreter;
};

} // namespace openmsx

#endif
