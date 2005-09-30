// $Id$

#ifndef COMPLETER_HH
#define COMPLETER_HH

#include <string>
#include <vector>
#include <set>

namespace openmsx {

class FileContext;
class CommandController;

class Completer
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
	Completer(CommandController& commandController,
	          const std::string& name);
	virtual ~Completer();

private:
	CommandController& commandController;
	const std::string name;
};

} // namespace openmsx

#endif
