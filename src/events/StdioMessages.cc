#include "StdioMessages.hh"
#include <iostream>

namespace openmsx {

void StdioMessages::log(CliComm::LogLevel level, std::string_view message, float fraction) noexcept
{
	auto& out = (level == CliComm::LogLevel::INFO) ? std::cout : std::cerr;
	out << toString(level) << ": " << message;
	if (level == CliComm::LogLevel::PROGRESS && fraction >= 0.0f) {
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
