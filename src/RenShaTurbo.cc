// $Id$

#include "RenShaTurbo.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "Autofire.hh"

namespace openmsx {

RenShaTurbo* RenShaTurbo::instance()
{
	static RenShaTurbo oneInstance;
	return &oneInstance;
}

RenShaTurbo::RenShaTurbo()
{
	try {
		Config *config = MSXConfig::instance()->getConfigById("RenShaTurbo");
		int min_ints = config->getParameterAsInt("min_ints", 47);
		int max_ints = config->getParameterAsInt("max_ints", 221);
		autofire = new Autofire(min_ints, max_ints, "renshaturbo");
	} catch (MSXException &e) {
		autofire = NULL;
	}
}

RenShaTurbo::~RenShaTurbo()
{
	delete autofire;
}

byte RenShaTurbo::getSignal(const EmuTime &time)
{
	if (autofire) {
		return autofire->getSignal(time);
	} else {
		return 0;
	}
}

} // namespace openmsx
