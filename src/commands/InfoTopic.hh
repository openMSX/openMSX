// $Id$

#ifndef __INFOTOPIC_HH__
#define __INFOTOPIC_HH__

#include <string>
#include <vector>
#include "CommandException.hh"

using std::string;
using std::vector;

namespace openmsx {

class InfoTopic
{
public:
	/** Show info on this topic
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  */
	virtual string execute(const vector<string> &tokens) const
		throw(CommandException) = 0;

	/** Print help for this topic.
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  */
	virtual string help(const vector<string> &tokens) const
		throw(CommandException) = 0;

	/** Attempt tab completion for this topic.
	  * Default implementation does nothing.
	  * @param tokens Tokenized command line;
	  *     tokens[1] is the topic.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(vector<string> &tokens) const
		throw() {}
};

} // namespace openmsx

#endif
