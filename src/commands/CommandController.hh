// $Id$

#ifndef __COMMANDCONTROLLER_HH__
#define __COMMANDCONTROLLER_HH__

#include <string>
#include <map>
#include <set>
#include <vector>
#include "Command.hh"

using std::string;
using std::map;
using std::set;
using std::vector;

namespace openmsx {

class CommandConsole;
class InfoCommand;
class Interpreter;

class CommandController
{
public:
	static CommandController& instance();

	/**
	 * (Un)register a command
	 */
	void registerCommand(Command* command, const string& str);
	void unregisterCommand(Command* command, const string& str);

	/**
	 * (Un)register a command completer, used to complete build-in TCL cmds
	 */
	void registerCompleter(CommandCompleter* completer, const string& str);
	void unregisterCompleter(CommandCompleter* completer, const string& str);

	/**
	 * Does a command with this name already exist?
	 */
	bool hasCommand(const string& command);

	/**
	 * Executes all defined auto commands
	 */
	void autoCommands();

	/**
	 * Returns true iff the command is complete
	 * (all braces, quotes, .. are balanced)
	 */
	bool isComplete(const string& command) const;
	
	/**
	 * Execute a given command
	 */
	string executeCommand(const string& command);

	/**
	 * Complete a given command
	 */
	void tabCompletion(string& command);

	/**
	 * TODO
	 */
	static void completeString(vector<string>& tokens, set<string>& set,
	                           bool caseSensitive = true);
	static void completeFileName(vector<string>& tokens);

	// should only be called by CommandConsole
	void setCommandConsole(CommandConsole* console);

private:
	CommandController();
	~CommandController();
	
	void split(const string& str, vector<string>& tokens, char delimiter);
	string join(const vector<string>& tokens, char delimiter);
	string removeEscaping(const string& str);
	void removeEscaping(const vector<string>& input,
	                    vector<string>& result, bool keepLastIfEmpty);
	string addEscaping(const string& str, bool quote, bool finished);

	void tabCompletion(vector<string>& tokens);
	static bool completeString2(string& str, set<string>& set,
	                            bool caseSensitive);
	static bool equal(const string& s1, const string& s2,
	                  bool caseSensitive);

	typedef map<string, Command*> CommandMap;
	typedef map<string, CommandCompleter*> CompleterMap;
	CommandMap commands;
	CompleterMap commandCompleters;

	// Commands
	class HelpCmd : public SimpleCommand {
	public:
		HelpCmd(CommandController& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		CommandController& parent;
	} helpCmd;
	friend class HelpCmd;

	CommandConsole* cmdConsole;
	
	InfoCommand& infoCommand;
	Interpreter& interpreter;
};

} // namespace openmsx

#endif
