#include "StdioMessages.hh"
#include <iostream>

using std::string;

namespace openmsx {

void StdioMessages::log(CliComm::LogLevel level, string_view message)
{
	auto levelStr = CliComm::getLevelStrings();
	((level == CliComm::INFO) ? std::cout : std::cerr) <<
		levelStr[level] << ": " << message << std::endl;

}

void StdioMessages::update(CliComm::UpdateType /*type*/, string_view /*machine*/,
                           string_view /*name*/, string_view /*value*/)
{
	// ignore
}

} // namespace openmsx
