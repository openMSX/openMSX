#ifndef INFOTOPIC_HH
#define INFOTOPIC_HH

#include "Completer.hh"
#include <span>
#include <string>
#include <vector>

namespace openmsx {

class TclObject;
class Interpreter;
class InfoCommand;

class InfoTopic : public Completer
{
public:
	InfoTopic(const InfoTopic&) = delete;
	InfoTopic(InfoTopic&&) = delete;
	InfoTopic& operator=(const InfoTopic&) = delete;
	InfoTopic& operator=(InfoTopic&&) = delete;

	/** Show info on this topic
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  * @param result The result of this topic must be assigned to this
	  *               parameter.
	  * @throw CommandException Thrown when there was an error while
	  *                         executing this InfoTopic.
	  */
	virtual void execute(std::span<const TclObject> tokens,
	                     TclObject& result) const = 0;

	/** Print help for this topic.
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  */
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override = 0;

	/** Attempt tab completion for this topic.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] Interpreter& getInterpreter() const final;

protected:
	InfoTopic(InfoCommand& infoCommand, const std::string& name);
	~InfoTopic();

	InfoCommand& infoCommand;
};

} // namespace openmsx

#endif
