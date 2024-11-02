#include "SpeedManager.hh"

namespace openmsx {

// class SpeedManager:

SpeedManager::SpeedManager(CommandController& commandController)
	: speedSetting(commandController, "speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100.0, 0.01, 10000.0, Setting::Save::NO)
	, fastforwardSpeedSetting(commandController, "fastforwardspeed",
	       "controls the emulation speed in fastforward mode: "
		   "higher is faster, 100 is normal speed",
	       2000.0, 0.01, 10000.0)
	, fastforwardSetting(commandController, "fastforward",
	       "select emulation speed:\n"
	       " on -> fastforward speed ('fastforwardspeed' setting)\n"
	       " off -> normal speed ('speed' setting)",
	       false, Setting::Save::NO)
{
	speedSetting.attach(*this);
	fastforwardSpeedSetting.attach(*this);
	fastforwardSetting.attach(*this);
}

SpeedManager::~SpeedManager()
{
	fastforwardSetting.detach(*this);
	fastforwardSpeedSetting.detach(*this);
	speedSetting.detach(*this);
}

void SpeedManager::updateSpeed()
{
	speed = (fastforwardSetting.getBoolean() ? fastforwardSpeedSetting : speedSetting)
	        .getDouble() * (1.0 / 100.0);
	notify();
}

void SpeedManager::update(const Setting& /*setting*/) noexcept
{
	updateSpeed();
}

} // namespace openmsx
