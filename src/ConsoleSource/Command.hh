// $Id$

#ifndef __COMMAND_HH__
#define __COMMAND_HH__

#include <string>
#include <vector>
#include "MSXException.hh"

class EmuTime;


class CommandException : public MSXException {
	public:
		CommandException(const std::string &desc_)
			: MSXException(desc_) {}
};

class Command
{
	/** 
	 * These are the functions the console can call after a device
	 * has registered commands with the console.
	 */
	public:
		/**
		 * called by the console when a command is typed
		 */
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time) = 0;
		/**
		 * called by the console when a help command is typed
		 */
		virtual void help(const std::vector<std::string> &tokens) const = 0;
		/**
		 * tab completeion for this command
		 * @param tokens A set of tokens, the last is incomplete, this
		 *               method tries to complete it.
		 * Default implementation does nothing
		 */
		virtual void tabCompletion(std::vector<std::string> &tokens) const {}

	/** 
	 * These are just helper functions
	 */
	protected:
		/**
		 * Prints a message to the console
		 */
		void print(const std::string &message) const;
};

#endif //_COMMAND_HH__
