#ifndef STDIOMESSAGES_HH
#define STDIOMESSAGES_HH

#include "CliListener.hh"

namespace openmsx {

class StdioMessages final : public CliListener
{
public:
	virtual void log(CliComm::LogLevel level, string_ref message);

	virtual void update(CliComm::UpdateType type, string_ref machine,
	                    string_ref name, string_ref value);
};

} // namespace openmsx

#endif
