#include "SpeedManager.hh"

#include "CommandController.hh"


namespace openmsx {

// class SpeedManager:

SpeedManager::SpeedManager(CommandController& commandController)
	: speedSetting(commandController, "speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000, Setting::DONT_SAVE)
	, turboSpeedSetting(commandController, "turbospeed",
	       "controls the emulation speed in turbo mode: "
		   "higher is faster, 100 is normal",
	       2000, 1, 1000000)
	, turboSetting(commandController, "turbo",
	       "select emulation speed:\n"
	       " on -> turbo speed ('turbospeed' setting)\n"
	       " off -> normal speed ('speed' setting)",
	       false, Setting::DONT_SAVE)
{
	speedSetting.attach(*this);
	turboSpeedSetting.attach(*this);
	turboSetting.attach(*this);
}

SpeedManager::~SpeedManager()
{
	turboSetting.detach(*this);
	turboSpeedSetting.detach(*this);
	speedSetting.detach(*this);
}

void SpeedManager::updateSpeed()
{
	speed = (turboSetting.getBoolean() ? turboSpeedSetting : speedSetting)
	        .getInt() / 100.0;
	notify();
}

void SpeedManager::update(const Setting& /*setting*/)
{
	updateSpeed();
}

} // namespace openmsx
