// $Id$

#ifndef __COMMANDCONTROLLER_HH__
#define __COMMANDCONTROLLER_HH__

#include <string>
#include <map>
#include <set>
#include <vector>
#include "Command.hh"

using std::string;
using std::multimap;
using std::set;
using std::vector;


namespace openmsx {

class CommandController
{
	public:
		static CommandController* instance();

		/**
		 * (Un)register a command
		 */
		void registerCommand(Command *commandObject,
		                     const string &str);
		void unregisterCommand(Command *commandObject,
		                       const string &str);

		/**
		 * Executes all defined auto commands
		 */
		void autoCommands();

		/**
		 * Execute a given command
		 */
		string executeCommand(const string &command);

		/**
		 * Complete a given command
		 */
		void tabCompletion(string &command);

		/**
		 * TODO
		 */
		static void completeString(vector<string> &tokens,
		                           set<string> &set);
		static void completeFileName(vector<string> &tokens);

	private:
		CommandController();
		~CommandController();
		
		void split(const string& str, vector<string>& tokens,
                           char delimiter);
		string join(const vector<string>& tokens, char delimiter);
		string removeEscaping(const string& str);
		void removeEscaping(const vector<string>& input,
		             vector<string>& output, bool keepLastIfEmpty);
		string addEscaping(const string& str, bool quote, bool finished);

		void tabCompletion(vector<string> &tokens);
		static bool completeString2(string& str, set<string>& set);

		struct ltstr {
			bool operator()(const string &s1, const string &s2) const {
				return s1 < s2;
			}
		};
		multimap<const string, Command*, ltstr> commands;

		// Commands
		class HelpCmd : public Command {
		public:
			virtual string execute(const vector<string> &tokens)
				throw(CommandException);
			virtual string help(const vector<string> &tokens) const
				throw();
			virtual void tabCompletion(vector<string> &tokens) const
				throw();
		};
		friend class HelpCmd;
		HelpCmd helpCmd;
};

} // namespace openmsx

#endif
