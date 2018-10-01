#include "CLIOption.hh"
#include "MSXException.hh"
#include <algorithm>

using std::string;

namespace openmsx {

// class CLIOption

string CLIOption::getArgument(const string& option, array_ref<string>& cmdLine) const
{
	if (cmdLine.empty()) {
		throw FatalError("Missing argument for option \"", option, '\"');
	}
	string argument = std::move(cmdLine.front());
	cmdLine.pop_front();
	return argument;
}

string CLIOption::peekArgument(const array_ref<string>& cmdLine) const
{
	return cmdLine.empty() ? string{} : cmdLine.front();
}

} // namespace openmsx
