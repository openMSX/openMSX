#include "CLIOption.hh"
#include "MSXException.hh"
#include <utility>

using std::string;

namespace openmsx {

// class CLIOption

string CLIOption::getArgument(const string& option, span<string>& cmdLine)
{
	if (cmdLine.empty()) {
		throw FatalError("Missing argument for option \"", option, '\"');
	}
	string argument = std::move(cmdLine.front());
	cmdLine = cmdLine.subspan(1);
	return argument;
}

string CLIOption::peekArgument(const span<string>& cmdLine)
{
	return cmdLine.empty() ? string{} : cmdLine.front();
}

} // namespace openmsx
