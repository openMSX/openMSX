// $Id$

#include "StdioMessages.hh"
#include <iostream>

using std::string;

namespace openmsx {

void StdioMessages::log(CliComm::LogLevel level, const string& message)
{
	const char* const* levelStr = CliComm::getLevelStrings();
	((level == CliComm::INFO) ? std::cout : std::cerr) <<
		levelStr[level] << ": " << message << std::endl;

}

void StdioMessages::update(CliComm::UpdateType /*type*/, const string& /*machine*/,
                           const string& /*name*/, const string& /*value*/)
{
	// ignore
}

} // namespace openmsx
