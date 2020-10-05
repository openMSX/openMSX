#ifndef CLILISTENER_HH
#define CLILISTENER_HH

#include "CliComm.hh"

namespace openmsx {

class CliListener
{
public:
	virtual ~CliListener() = default;

	virtual void log(CliComm::LogLevel level, std::string_view message) = 0;

	virtual void update(CliComm::UpdateType type, std::string_view machine,
	                    std::string_view name, std::string_view value) = 0;

protected:
	CliListener() = default;
};

} // namespace openmsx

#endif
