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
class MSXConfig;
class CliCommOutput;

class CommandController
{
public:
	static CommandController& instance();

	/**
	 * (Un)register a command
	 */
	void registerCommand(Command* commandObject, const string& str);
	void unregisterCommand(Command* commandObject, const string& str);
	/**
	 * Does a command with this name already exist?
	 */
	bool hasCommand(const string& command);

	/**
	 * Executes all defined auto commands
	 */
	void autoCommands();

	/**
	 * Execute a given command
	 */
	string executeCommand(const string& command) throw (CommandException);

	/**
	 * Complete a given command
	 */
	void tabCompletion(string& command);

	/**
	 * TODO
	 */
	static void completeString(vector<string>& tokens, set<string>& set);
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
	                    vector<string>& output, bool keepLastIfEmpty);
	string addEscaping(const string& str, bool quote, bool finished);

	void tabCompletion(vector<string>& tokens);
	static bool completeString2(string& str, set<string>& set);

	typedef map<string, Command*> CommandMap;
	CommandMap commands;

	// Commands
	class HelpCmd : public Command {
	public:
		HelpCmd(CommandController& parent);
		virtual string execute(const vector<string>& tokens)
			throw(CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
		virtual void tabCompletion(vector<string>& tokens) const
			throw();
	private:
		CommandController& parent;
	} helpCmd;
	friend class HelpCmd;

	CommandConsole* cmdConsole;
	
	InfoCommand& infoCommand;
	MSXConfig& msxConfig;
	CliCommOutput& output;
};

} // namespace openmsx

#endif
