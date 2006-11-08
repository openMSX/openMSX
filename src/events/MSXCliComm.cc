// $Id: CliComm.cc 5779 2006-10-14 16:17:32Z m9710797 $

#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXMotherBoard.hh"
#include "MSXCommandController.hh"

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
	cliComm.update(type, motherBoard.getMSXCommandController().getNamespace(),
	               name, value);
}

} // namespace openmsx
