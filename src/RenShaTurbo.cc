// $Id$

#include "RenShaTurbo.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "Autofire.hh"

namespace openmsx {

RenShaTurbo& RenShaTurbo::instance()
{
	static RenShaTurbo oneInstance;
	return oneInstance;
}

RenShaTurbo::RenShaTurbo()
{
	Config* config = SettingsConfig::instance().findConfigById("RenShaTurbo");
	if (config) {
		int min_ints = config->getParameterAsInt("min_ints", 47);
		int max_ints = config->getParameterAsInt("max_ints", 221);
		autofire.reset(new Autofire(min_ints, max_ints, "renshaturbo"));
	}
}

RenShaTurbo::~RenShaTurbo()
{
}

byte RenShaTurbo::getSignal(const EmuTime &time)
{
	return (autofire.get()) ? autofire->getSignal(time)
	                        : 0;
}

} // namespace openmsx
