#ifndef CLILISTENER_HH
#define CLILISTENER_HH

#include "CliComm.hh"

namespace openmsx {

class CliListener
{
public:
	virtual ~CliListener() {}

	virtual void log(CliComm::LogLevel level, string_view message) = 0;

	virtual void update(CliComm::UpdateType type, string_view machine,
	                    string_view name, string_view value) = 0;

protected:
	CliListener() {}
};

} // namespace openmsx

#endif
