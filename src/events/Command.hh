// $Id$

#ifndef __COMMAND_HH__
#define __COMMAND_HH__

#include <string>
#include <vector>
#include "MSXException.hh"

using std::string;
using std::vector;


class CommandException : public MSXException {
	public:
		CommandException(const string &desc_)
			: MSXException(desc_) {}
};

class Command
{
	public:
		/** Execute this command.
		  * @param tokens Tokenized command line;
		  * 	tokens[0] is the command itself.
		  */
		virtual void execute(const vector<string> &tokens) = 0;

		/** Print help for this command.
		  */
		virtual void help(const vector<string> &tokens) const = 0;

		/** Attempt tab completion for this command.
		  * Default implementation does nothing.
		  * @param tokens Tokenized command line;
		  * 	tokens[0] is the command itself.
		  * 	The last token is incomplete, this method tries to complete it.
		  */
		virtual void tabCompletion(vector<string> &tokens) const {}

	protected:
		/** Convenience method to print a message to the console.
		  */
		void print(const string &message) const;
};

#endif //_COMMAND_HH__
