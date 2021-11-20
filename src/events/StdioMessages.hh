#ifndef STDIOMESSAGES_HH
#define STDIOMESSAGES_HH

#include "CliListener.hh"

namespace openmsx {

class StdioMessages final : public CliListener
{
public:
	void log(CliComm::LogLevel level, std::string_view message) noexcept override;

	void update(CliComm::UpdateType type, std::string_view machine,
	            std::string_view name, std::string_view value) noexcept override;
};

} // namespace openmsx

#endif
