#ifndef STDIOMESSAGES_HH
#define STDIOMESSAGES_HH

#include "CliListener.hh"

namespace openmsx {

class StdioMessages final : public CliListener
{
public:
	void log(CliComm::LogLevel level, string_ref message) override;

	void update(CliComm::UpdateType type, string_ref machine,
	            string_ref name, string_ref value) override;
};

} // namespace openmsx

#endif
