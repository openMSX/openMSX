#include "StdioMessages.hh"
#include <iostream>

using std::string;

namespace openmsx {

void StdioMessages::log(CliComm::LogLevel level, string_ref message)
{
	auto levelStr = CliComm::getLevelStrings();
	((level == CliComm::INFO) ? std::cout : std::cerr) <<
		levelStr[level] << ": " << message << std::endl;

}

void StdioMessages::update(CliComm::UpdateType /*type*/, string_ref /*machine*/,
                           string_ref /*name*/, string_ref /*value*/)
{
	// ignore
}

} // namespace openmsx
