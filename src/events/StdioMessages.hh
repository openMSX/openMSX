// $Id$

#ifndef STDIOMESSAGES_HH
#define STDIOMESSAGES_HH

#include "CliListener.hh"

namespace openmsx {

class StdioMessages : public CliListener
{
public:
	virtual void log(CliComm::LogLevel level, const std::string& message);

	virtual void update(CliComm::UpdateType type, const std::string& machine,
	                    const std::string& name, const std::string& value);
};

} // namespace openmsx

#endif
