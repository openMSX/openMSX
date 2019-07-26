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
	if (auto* v = lookup(prevValues[type], name)) {
		if (*v == value) {
			return;
		}
		*v = value;
	} else {
		prevValues[type].emplace_noDuplicateCheck(name, value);
	}
	cliComm.updateHelper(type, motherBoard.getMachineID(), name, value);
}

} // namespace openmsx
