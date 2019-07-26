#include "StdioMessages.hh"
#include <iostream>

using std::string;

namespace openmsx {

void StdioMessages::log(CliComm::LogLevel level, std::string_view message)
{
	auto levelStr = CliComm::getLevelStrings();
	((level == CliComm::INFO) ? std::cout : std::cerr) <<
		levelStr[level] << ": " << message << '\n' << std::flush;

}

void StdioMessages::update(CliComm::UpdateType /*type*/, std::string_view /*machine*/,
                           std::string_view /*name*/, std::string_view /*value*/)
{
	// ignore
}

} // namespace openmsx
