#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

MSXCliComm::MSXCliComm(MSXMotherBoard& motherBoard_, GlobalCliComm& cliComm_)
	: motherBoard(motherBoard_)
	, cliComm(cliComm_)
{
}

void MSXCliComm::log(LogLevel level, string_view message)
{
	cliComm.log(level, message);
}

void MSXCliComm::update(UpdateType type, string_view name, string_view value)
{
	assert(type < NUM_UPDATES);
	auto it = prevValues[type].find(name);
	if (it != end(prevValues[type])) {
		if (it->second == value) {
			return;
		}
		it->second = value.str();
	} else {
		prevValues[type].emplace_noDuplicateCheck(name.str(), value.str());
	}
	cliComm.updateHelper(type, motherBoard.getMachineID(), name, value);
}

} // namespace openmsx
