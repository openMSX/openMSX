#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

MSXCliComm::MSXCliComm(MSXMotherBoard& motherBoard_, GlobalCliComm& cliComm_)
	: motherBoard(motherBoard_)
	, cliComm(cliComm_)
{
}

void MSXCliComm::log(LogLevel level, std::string_view message)
{
	cliComm.log(level, message);
}

void MSXCliComm::update(UpdateType type, std::string_view name, std::string_view value)
{
	assert(type < NUM_UPDATES);
	cliComm.updateHelper(type, motherBoard.getMachineID(), name, value);
}

void MSXCliComm::updateFiltered(UpdateType type, std::string_view name, std::string_view value)
{
	assert(type < NUM_UPDATES);
	if (auto [it, inserted] = prevValues[type].try_emplace(name, value);
	    !inserted) { // was already present ..
		if (it->second == value) {
			return; // .. with the same value
		} else {
			it->second = value; // .. but with a different value
		}
	}
	cliComm.updateHelper(type, motherBoard.getMachineID(), name, value);
}

} // namespace openmsx
