#ifndef INFOTOPIC_HH
#define INFOTOPIC_HH

#include "Completer.hh"
#include "array_ref.hh"
#include <string>
#include <vector>

namespace openmsx {

class TclObject;
class Interpreter;
class InfoCommand;

class InfoTopic : public Completer
{
public:
	/** Show info on this topic
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  * @param result The result of this topic must be assigned to this
	  *               parameter.
	  * @throw CommandException Thrown when there was an error while
	  *                         executing this InfoTopic.
	  */
	virtual void execute(array_ref<TclObject> tokens,
	                     TclObject& result) const = 0;

	/** Print help for this topic.
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  */
	std::string help(const std::vector<std::string>& tokens) const override = 0;

	/** Attempt tab completion for this topic.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	void tabCompletion(std::vector<std::string>& tokens) const override;

	Interpreter& getInterpreter() const;

protected:
	InfoTopic(InfoCommand& infoCommand, const std::string& name);
	~InfoTopic();

private:
	InfoCommand& infoCommand;
};

} // namespace openmsx

#endif
