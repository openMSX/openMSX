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
		virtual ~CommandController();
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
		void executeCommand(const string &command);

		/**
		 * Complete a given command
		 */
		void tabCompletion(string &command);
		void tabCompletion(vector<string> &tokens);

		/**
		 * TODO
		 */
		static void completeString(vector<string> &tokens,
		                           set<string> &set);
		static void completeFileName(vector<string> &tokens);

	private:
		CommandController();
		void tokenize(const string &str,
		              vector<string> &tokens,
		              const string &delimiters = " ");
		static bool completeString2(string &string,
		                            set<string> &set);

		struct ltstr {
			bool operator()(const string &s1, const string &s2) const {
				return s1 < s2;
			}
		};
		multimap<const string, Command*, ltstr> commands;

		// Commands
		class HelpCmd : public Command {
		public:
			virtual void execute(const vector<string> &tokens);
			virtual void help(const vector<string> &tokens) const;
			virtual void tabCompletion(vector<string> &tokens) const;
		};
		friend class HelpCmd;
		HelpCmd helpCmd;
};

} // namespace openmsx

#endif
