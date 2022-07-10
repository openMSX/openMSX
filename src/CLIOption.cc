#include "CLIOption.hh"
#include "MSXException.hh"
#include <utility>

namespace openmsx {

// class CLIOption

std::string CLIOption::getArgument(const std::string& option, std::span<std::string>& cmdLine)
{
	if (cmdLine.empty()) {
		throw FatalError("Missing argument for option \"", option, '\"');
	}
	std::string argument = std::move(cmdLine.front());
	cmdLine = cmdLine.subspan(1);
	return argument;
}

std::string CLIOption::peekArgument(const std::span<std::string>& cmdLine)
{
	return cmdLine.empty() ? std::string{} : cmdLine.front();
}

} // namespace openmsx
