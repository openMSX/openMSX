#ifndef CLILISTENER_HH
#define CLILISTENER_HH

#include "CliComm.hh"

namespace openmsx {

class CliListener
{
public:
	virtual ~CliListener() {}

	virtual void log(CliComm::LogLevel level, string_ref message) = 0;

	virtual void update(CliComm::UpdateType type, string_ref machine,
	                    string_ref name, string_ref value) = 0;

protected:
	CliListener() {}
};

} // namespace openmsx

#endif
