// $Id$

#include "RenShaTurbo.hh"
#include "HardwareConfig.hh"
#include "Autofire.hh"

namespace openmsx {

RenShaTurbo::RenShaTurbo()
{
	const XMLElement* config = HardwareConfig::instance().findChild("RenShaTurbo");
	if (config) {
		int min_ints = config->getChildDataAsInt("min_ints", 47);
		int max_ints = config->getChildDataAsInt("max_ints", 221);
		autofire.reset(new Autofire(min_ints, max_ints, "renshaturbo"));
	}
}

RenShaTurbo::~RenShaTurbo()
{
}

byte RenShaTurbo::getSignal(const EmuTime& time)
{
	return autofire.get() ? autofire->getSignal(time) : 0;
}

} // namespace openmsx
