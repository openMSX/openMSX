// $Id$

#ifndef __COMMANDCONTROLLER_HH__
#define __COMMANDCONTROLLER_HH__

#include <string>
#include <map>
#include <set>
#include <vector>
#include "Command.hh"


class CommandController
{
	public:
		virtual ~CommandController();
		static CommandController* instance();

		/**
		 * (Un)register a command
		 */
		void registerCommand(Command *commandObject,
		                     const std::string &str);
		void unregisterCommand(Command *commandObject,
		                       const std::string &str);

		/**
		 * Executes all defined auto commands
		 */
		void autoCommands();

		/**
		 * Execute a given command
		 */
		void executeCommand(const std::string &command);

		/**
		 * Complete a given command
		 */
		void tabCompletion(std::string &command);
		void tabCompletion(std::vector<std::string> &tokens);

		/**
		 * TODO
		 */
		static void completeString(std::vector<std::string> &tokens,
		                           std::set<std::string> &set);
		static void completeFileName(std::vector<std::string> &tokens);

	private:
		CommandController();
		void tokenize(const std::string &str,
		              std::vector<std::string> &tokens,
		              const std::string &delimiters = " ");
		static bool completeString2(std::string &string,
		                            std::set<std::string> &set);

		struct ltstr {
			bool operator()(const std::string &s1, const std::string &s2) const {
				return s1 < s2;
			}
		};
		std::multimap<const std::string, Command*, ltstr> commands;

		// Commands
		class HelpCmd : public Command {
		public:
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help(const std::vector<std::string> &tokens) const;
			virtual void tabCompletion(std::vector<std::string> &tokens) const;
		};
		friend class HelpCmd;
		HelpCmd helpCmd;
};

#endif
