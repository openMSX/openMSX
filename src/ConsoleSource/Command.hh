// $Id$

#ifndef __COMMAND_HH__
#define __COMMAND_HH__

#include <string>
#include <vector>
#include "../MSXException.hh"


class CommandException : public MSXException {
	public:
		CommandException(const std::string &desc) : MSXException(desc) {}
};

/** These are the functions the console can call after a device
  * has registered commands with the console.
  */
class Command
{
	public:
		/**
		 * called by the console when a command is typed
		 */
		virtual void execute(const std::vector<std::string> &tokens)=0;
		/**
		 * called by the console when a help command is typed
		 */
		virtual void help   (const std::vector<std::string> &tokens)=0;
		/**
		 * tab completeion for this command
		 * @param tokens A set of tokens, the last is incomplete, this
		 *               method tries to complete it.
		 * Default implementation does nothing
		 */
		virtual void tabCompletion(const std::vector<std::string> &tokens) {}
};

#endif //_COMMAND_HH__
