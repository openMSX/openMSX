// $Id$

#include "RenShaTurbo.hh"
#include "XMLElement.hh"
#include "Autofire.hh"

namespace openmsx {

RenShaTurbo::RenShaTurbo(CommandController& commandController,
                         const XMLElement& machineConfig)
{
	if (const XMLElement* config = machineConfig.findChild("RenShaTurbo")) {
		int min_ints = config->getChildDataAsInt("min_ints", 47);
		int max_ints = config->getChildDataAsInt("max_ints", 221);
		autofire.reset(new Autofire(commandController, min_ints,
		                            max_ints, "renshaturbo"));
	}
}

RenShaTurbo::~RenShaTurbo()
{
}

bool RenShaTurbo::getSignal(const EmuTime& time)
{
	return autofire.get() ? autofire->getSignal(time) : false;
}

} // namespace openmsx
