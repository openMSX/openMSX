// $Id$

#include "Command.hh"
#include "CommandArgument.hh"

namespace openmsx {

void SimpleCommand::execute(const vector<CommandArgument>& tokens,
                            CommandArgument& result)
{
	vector<string> strings;
	strings.reserve(tokens.size());
	for (vector<CommandArgument>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		strings.push_back(it->getString());
	}
	result.setString(execute(strings));
}

} // namespace openmsx
