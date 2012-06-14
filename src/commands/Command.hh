// $Id$

#ifndef COMMAND_HH
#define COMMAND_HH

#include "Completer.hh"
#include "string_ref.hh"
#include <vector>

namespace openmsx {

class CommandController;
class GlobalCommandController;
class Interpreter;
class TclObject;

class CommandCompleter : public Completer
{
protected:
	CommandCompleter(CommandController& commandController,
	                 string_ref name);
	virtual ~CommandCompleter();

	CommandController& getCommandController() const;
	GlobalCommandController& getGlobalCommandController() const;
	Interpreter& getInterpreter() const;

private:
	CommandController& commandController;
};


class Command : public CommandCompleter
{
public:
	/** Execute this command.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  * @param result The result of the command must be assigned to this
	  *               parameter.
	  * @throws CommandException Thrown when there was an error while
	  *                          executing this command.
	  */
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);

	/** Alternative for the execute() method above.
	  * It has a simpler interface, but performance is a bit lower.
	  * Subclasses should override either this method or the one above.
	  */
	virtual std::string execute(const std::vector<std::string>& tokens);

	/** Attempt tab completion for this command.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

protected:
	Command(CommandController& commandController,
	        string_ref name);
	virtual ~Command();
};

} // namespace openmsx

#endif
