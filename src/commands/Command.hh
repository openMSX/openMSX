#ifndef COMMAND_HH
#define COMMAND_HH

#include "Completer.hh"
#include "span.hh"
#include "strCat.hh"
#include "CommandException.hh"
#include <string_view>
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

	[[nodiscard]] CommandController& getCommandController() const { return commandController; }
	[[nodiscard]] Interpreter& getInterpreter() const final;

protected:
	CommandCompleter(CommandController& controller, std::string_view name);
	~CommandCompleter();

	[[nodiscard]] GlobalCommandController& getGlobalCommandController() const;
	[[nodiscard]] CliComm& getCliComm() const;

private:
	CommandController& commandController;
};


class Command : public CommandCompleter
{
	struct UnknownSubCommand {};

public:
	/** Execute this command.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  * @param result The result of the command must be assigned to this
	  *               parameter.
	  * @throws CommandException Thrown when there was an error while
	  *                          executing this command.
	  */
	virtual void execute(span<const TclObject> tokens, TclObject& result) = 0;

	/** Attempt tab completion for this command.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	void tabCompletion(std::vector<std::string>& tokens) const override;

	// see comments in MSXMotherBoard::loadMachineCommand
	void setAllowedInEmptyMachine(bool value) { allowInEmptyMachine = value; }
	[[nodiscard]] bool isAllowedInEmptyMachine() const { return allowInEmptyMachine; }

	// used by Interpreter::(un)registerCommand()
	void setToken(void* token_) { assert(!token); token = token_; }
	[[nodiscard]] void* getToken() const { return token; }

	// helper to delegate to a subcommand
	template<typename... Args>
	void executeSubCommand(std::string_view subcmd, Args&&... args) {
		try {
			executeSubCommandImpl(subcmd, std::forward<Args>(args)...);
		} catch (UnknownSubCommand) {
			unknownSubCommand(subcmd, std::forward<Args>(args)...);
		}
	}

protected:
	Command(CommandController& controller, std::string_view name);
	~Command();

private:
	template<typename Func, typename... Args>
	void executeSubCommandImpl(std::string_view subcmd, std::string_view candidate, Func func, Args&&... args) {
		if (subcmd == candidate) {
			func();
		} else {
			executeSubCommandImpl(subcmd, std::forward<Args>(args)...);
		}
	}
	void executeSubCommandImpl(std::string_view /*subcmd*/) {
		throw UnknownSubCommand{}; // exhausted all possible candidates
	}

	template<typename Func, typename... Args>
	void unknownSubCommand(std::string_view subcmd, std::string_view firstCandidate, Func /*func*/, Args&&... args) {
		unknownSubCommandImpl(strCat("Unknown subcommand '", subcmd, "'. Must be one of '", firstCandidate, '\''),
		                      std::forward<Args>(args)...);
	}
	template<typename Func, typename... Args>
	void unknownSubCommandImpl(std::string message, std::string_view candidate, Func /*func*/, Args&&... args) {
		strAppend(message, ", '", candidate, '\'');
		unknownSubCommandImpl(message, std::forward<Args>(args)...);
		throw SyntaxError();
	}
	template<typename Func>
	void unknownSubCommandImpl(std::string message, std::string_view lastCandidate, Func /*func*/) {
		strAppend(message, " or '", lastCandidate, "'.");
		throw CommandException(message);
	}

private:
	bool allowInEmptyMachine;
	void* token;
};

} // namespace openmsx

#endif
