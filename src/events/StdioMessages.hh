#ifndef STDIOMESSAGES_HH
#define STDIOMESSAGES_HH

#include "CliListener.hh"

namespace openmsx {

class StdioMessages final : public CliListener
{
public:
	void log(CliComm::LogLevel level, string_view message) override;

	void update(CliComm::UpdateType type, string_view machine,
	            string_view name, string_view value) override;
};

} // namespace openmsx

#endif
