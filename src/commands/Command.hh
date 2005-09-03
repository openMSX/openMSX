// $Id$

#ifndef COMMAND_HH
#define COMMAND_HH

#include <string>
#include <vector>
#include <set>

namespace openmsx {

class FileContext;
class TclObject;
class CommandController;

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

	void completeString(std::vector<std::string>& tokens,
	                    std::set<std::string>& set,
	                    bool caseSensitive = true) const;
	void completeFileName(std::vector<std::string>& tokens) const;
	void completeFileName(std::vector<std::string>& tokens,
	                      const FileContext& context,
	                      const std::set<std::string>& extra) const;

	CommandController& getCommandController() const;
	const std::string& getName() const;
	
protected:
	CommandCompleter(CommandController& commandController,
	                 const std::string& name);
	virtual ~CommandCompleter();

private:
	CommandController& commandController;
	const std::string name;
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
	SimpleCommand(CommandController& commandController, const std::string& name);
	virtual ~SimpleCommand();
};

} // namespace openmsx

#endif
