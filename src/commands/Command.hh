// $Id$

#ifndef __COMMAND_HH__
#define __COMMAND_HH__

#include <string>
#include <vector>
#include "CommandException.hh"

using std::string;
using std::vector;

namespace openmsx {

class CommandResult;

class CommandCompleter
{
public:
	virtual ~CommandCompleter() {}

	/** Attempt tab completion for this command.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  * 	The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(vector<string>& tokens) const throw() = 0;
};


class Command : public CommandCompleter
{
public:
	/** Execute this command.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  */
	virtual void execute(const vector<string>& tokens, CommandResult& result)
		throw (CommandException) = 0;

	/** Print help for this command.
	  */
	virtual string help(const vector<string>& tokens) const
		throw(CommandException) = 0;

	/** Attempt tab completion for this command.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  * 	The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(vector<string>& tokens) const
		throw() {}
};

/**
 * Simplified Command class for commands that just need to
 * return a (small) string
 */
class SimpleCommand : public Command
{
public:
	virtual string execute(const vector<string>& tokens)
		throw (CommandException) = 0;
	
	virtual void execute(const vector<string>& tokens,
	                     CommandResult& result)
		throw (CommandException);
};

} // namespace openmsx

#endif //_COMMAND_HH__
