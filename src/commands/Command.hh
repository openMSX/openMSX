// $Id$

#ifndef __COMMAND_HH__
#define __COMMAND_HH__

#include <string>
#include <vector>

namespace openmsx {

class CommandArgument;

class CommandCompleter
{
public:
	/** Print help for this command.
	  */
	virtual std::string help(const std::vector<std::string>& tokens) const = 0;

	/** Attempt tab completion for this command.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  * 	The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;
};


class Command : public CommandCompleter
{
public:
	/** Execute this command.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  * @param result The result of the command must be assigned to this
	  *               parameter.
	  * @throws CommandException Thrown when there was an error while
	  *                          executing this command.
	  */
	virtual void execute(const std::vector<CommandArgument>& tokens,
	                     CommandArgument& result) = 0;

	/** Attempt tab completion for this command.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  * 	The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
};

/**
 * Simplified Command class for commands that just need to
 * return a (small) string
 */
class SimpleCommand : public Command
{
public:
	virtual std::string execute(const std::vector<std::string>& tokens) = 0;
	
	virtual void execute(const std::vector<CommandArgument>& tokens,
	                     CommandArgument& result);
};

} // namespace openmsx

#endif //_COMMAND_HH__
