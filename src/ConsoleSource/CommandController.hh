// $Id$ 

#ifndef __COMMANDCONTROLLER_HH__
#define __COMMANDCONTROLLER_HH__

#include <string>
#include <map>
#include <vector>

// forward declaration
class Command;


class CommandController
{
	public:
		virtual ~CommandController();
		static CommandController* instance();

		/**
		 * TODO
		 */
		void registerCommand(Command &commandObject, const std::string &str);
		/**
		 * TODO
		 */
		void unRegisterCommand(const std::string &str);
		
		/**
		 * Executes all defined auto commands
		 */
		void autoCommands();
		
		/**
		 * Execute a given command
		 */
		void executeCommand(const std::string &command);
		
	private:
		CommandController();
		void tokenize(const std::string &str, vector<std::string> &tokens, const std::string &delimiters = " ");

		static CommandController* oneInstance;
		
		struct ltstr {
			bool operator()(const std::string &s1, const std::string &s2) const {
				return s1 < s2;
			}
		};
		std::map<const std::string, Command*, ltstr> commands;
};

#endif
