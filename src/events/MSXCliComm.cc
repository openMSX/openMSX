// $Id$

#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

MSXCliComm::MSXCliComm(MSXMotherBoard& motherBoard_, GlobalCliComm& cliComm_)
	: motherBoard(motherBoard_)
	, cliComm(cliComm_)
{
}

void MSXCliComm::log(LogLevel level, string_ref message)
{
	cliComm.log(level, message);
}

void MSXCliComm::update(UpdateType type, string_ref name, string_ref value)
{
	assert(type < NUM_UPDATES);
	auto it = prevValues[type].find(name);
	if (it != prevValues[type].end()) {
		if (it->second == value) {
			return;
		}
		it->second.assign(value.data(), value.size());
	} else {
		prevValues[type][name].assign(value.data(), value.size());
	}
	cliComm.updateHelper(type, motherBoard.getMachineID(), name, value);
}

} // namespace openmsx
