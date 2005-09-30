// $Id$

#ifndef COMMAND_HH
#define COMMAND_HH

#include "Completer.hh"
#include <string>
#include <vector>

namespace openmsx {

class TclObject;
class CommandController;

class CommandCompleter : public Completer
{
protected:
	CommandCompleter(CommandController& commandController,
	                 const std::string& name);
	virtual ~CommandCompleter();
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
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result) = 0;

	/** Attempt tab completion for this command.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  * 	tokens[0] is the command itself.
	  * 	The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

protected:
	Command(CommandController& commandController, const std::string& name);
	virtual ~Command();
};

/**
 * Simplified Command class for commands that just need to
 * return a (small) string
 */
class SimpleCommand : public Command
{
public:
	virtual std::string execute(const std::vector<std::string>& tokens) = 0;

	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);

protected:
	SimpleCommand(CommandController& commandController,
	              const std::string& name);
	virtual ~SimpleCommand();
};

} // namespace openmsx

#endif
