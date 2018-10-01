#ifndef COMMAND_HH
#define COMMAND_HH

#include "Completer.hh"
#include "array_ref.hh"
#include "string_view.hh"
#include <vector>

namespace openmsx {

class CommandController;
class GlobalCommandController;
class Interpreter;
class TclObject;
class CliComm;

class CommandCompleter : public Completer
{
public:
	CommandCompleter(const CommandCompleter&) = delete;
	CommandCompleter& operator=(const CommandCompleter&) = delete;

	CommandController& getCommandController() const { return commandController; }
	Interpreter& getInterpreter() const;

protected:
	CommandCompleter(CommandController& controller, string_view name);
	~CommandCompleter();

	GlobalCommandController& getGlobalCommandController() const;
	CliComm& getCliComm() const;

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
	virtual void execute(array_ref<TclObject> tokens, TclObject& result) = 0;

	/** Attempt tab completion for this command.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	void tabCompletion(std::vector<std::string>& tokens) const override;

	// see comments in MSXMotherBoard::loadMachineCommand
	void setAllowedInEmptyMachine(bool value) { allowInEmptyMachine = value; }
	bool isAllowedInEmptyMachine() const { return allowInEmptyMachine; }

	// used by Interpreter::(un)registerCommand()
	void setToken(void* token_) { assert(!token); token = token_; }
	void* getToken() const { return token; }

protected:
	Command(CommandController& controller, string_view name);
	~Command();

private:
	bool allowInEmptyMachine;
	void* token;
};

} // namespace openmsx

#endif
