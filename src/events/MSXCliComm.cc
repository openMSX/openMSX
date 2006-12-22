// $Id$

#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXMotherBoard.hh"

using std::string;

namespace openmsx {

MSXCliComm::MSXCliComm(MSXMotherBoard& motherBoard_, GlobalCliComm& cliComm_)
	: motherBoard(motherBoard_)
	, cliComm(cliComm_)
{
}

void MSXCliComm::log(LogLevel level, const string& message)
{
	cliComm.log(level, message);
}

void MSXCliComm::update(UpdateType type, const string& name,
                        const string& value)
{
	cliComm.update(type, motherBoard.getMachineID(), name, value);
}

} // namespace openmsx
