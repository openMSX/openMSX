// $Id$

#include "ThrottleManager.hh"
#include "BooleanSetting.hh"

namespace openmsx {

ThrottleManager::ThrottleManager(CommandController& commandController)
	: loading(0), throttle(true)
{
	throttleSetting.reset(new BooleanSetting(commandController, "throttle",
		"controls speed throttling", true, Setting::DONT_SAVE));
	fullSpeedLoadingSetting.reset(new BooleanSetting(commandController,
		"fullspeedwhenloading",
		"sets openMSX to full speed when the MSX is loading", false));

	throttleSetting->attach(*this);
	fullSpeedLoadingSetting->attach(*this);
}

ThrottleManager::~ThrottleManager()
{
	throttleSetting->detach(*this);
	fullSpeedLoadingSetting->detach(*this);
}

void ThrottleManager::updateStatus()
{
	bool newThrottle = throttleSetting->getValue() &&
	                   (!loading || !fullSpeedLoadingSetting->getValue());
	if (throttle != newThrottle) {
		throttle = newThrottle;
		notify();
	}
}

bool ThrottleManager::isThrottled() const
{
	return throttle;
}

void ThrottleManager::indicateLoadingState(bool state)
{
	if (state) {
		++loading;
	} else { 
		--loading;
	}
	assert (loading >= 0);
	updateStatus();
}

void ThrottleManager::update(const Setting& /*setting*/)
{
	updateStatus();
}

} // namespace openmsx
