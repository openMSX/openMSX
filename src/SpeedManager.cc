#include "SpeedManager.hh"

#include "CommandController.hh"


namespace openmsx {

// class SpeedManager:

SpeedManager::SpeedManager(CommandController& commandController)
	: speedSetting(commandController, "speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000, Setting::DONT_SAVE)
{
	speedSetting.attach(*this);
}

SpeedManager::~SpeedManager()
{
	speedSetting.detach(*this);
}

void SpeedManager::updateSpeed()
{
	speed = speedSetting.getInt() / 100.0;
	notify();
}

void SpeedManager::update(const Setting& /*setting*/)
{
	updateSpeed();
}

} // namespace openmsx
