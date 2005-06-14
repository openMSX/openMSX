// $Id$

#include "Command.hh"
#include "TclObject.hh"

using std::vector;
using std::string;

namespace openmsx {

void Command::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

void SimpleCommand::execute(const vector<TclObject*>& tokens,
                            TclObject& result)
{
	vector<string> strings;
	strings.reserve(tokens.size());
	for (vector<TclObject*>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		strings.push_back((*it)->getString());
	}
	result.setString(execute(strings));
}

} // namespace openmsx
