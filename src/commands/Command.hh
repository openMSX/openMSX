// $Id$

#ifndef __COMMAND_HH__
#define __COMMAND_HH__

#include <string>
#include <vector>
#include "MSXException.hh"

using std::string;
using std::vector;


namespace openmsx {

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
		virtual string execute(const vector<string> &tokens) = 0;

		/** Print help for this command.
		  */
		virtual string help(const vector<string> &tokens) const = 0;

		/** Attempt tab completion for this command.
		  * Default implementation does nothing.
		  * @param tokens Tokenized command line;
		  * 	tokens[0] is the command itself.
		  * 	The last token is incomplete, this method tries to complete it.
		  */
		virtual void tabCompletion(vector<string> &tokens) const {}
};

} // namespace openmsx

#endif //_COMMAND_HH__
