#include "StdioMessages.hh"
#include <iostream>

namespace openmsx {

void StdioMessages::log(CliComm::LogLevel level, std::string_view message, float fraction) noexcept
{
	auto levelStr = CliComm::getLevelStrings();
	auto& out = (level == CliComm::INFO) ? std::cout : std::cerr;
	out << levelStr[level] << ": " << message;
	if (level == CliComm::PROGRESS && fraction >= 0.0f) {
		out << "... " << int(100.0f * fraction) << '%';
	}
	out << '\n' << std::flush;
}

void StdioMessages::update(CliComm::UpdateType /*type*/, std::string_view /*machine*/,
                           std::string_view /*name*/, std::string_view /*value*/) noexcept
{
	// ignore
}

} // namespace openmsx
