// $Id$

#ifndef MSXCLICOMM_HH
#define MSXCLICOMM_HH

#include "CliComm.hh"
#include "noncopyable.hh"
#include <map>

namespace openmsx {

class MSXMotherBoard;
class GlobalCliComm;

class MSXCliComm : public CliComm, private noncopyable
{
public:
	MSXCliComm(MSXMotherBoard& motherBoard, GlobalCliComm& cliComm);

	virtual void log(LogLevel level, const std::string& message);
	virtual void update(UpdateType type, const std::string& name,
	                    const std::string& value);

private:
	MSXMotherBoard& motherBoard;
	GlobalCliComm& cliComm;

	std::map<std::string, std::string> prevValues[NUM_UPDATES];
};

} // namespace openmsx

#endif
