// $Id$

#ifndef CLILISTENER_HH
#define CLILISTENER_HH

#include "CliComm.hh"
#include <string>

namespace openmsx {

class CliListener
{
public:
	virtual ~CliListener() {}

	virtual void log(CliComm::LogLevel level, const std::string& message) = 0;

	virtual void update(CliComm::UpdateType type, const std::string& machine,
	                    const std::string& name, const std::string& value) = 0;

protected:
	CliListener() {}
};

} // namespace openmsx

#endif
