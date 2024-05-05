#include "ThrottleManager.hh"

namespace openmsx {

// class ThrottleManager:

ThrottleManager::ThrottleManager(CommandController& commandController)
	: throttleSetting(
		commandController, "throttle",
		"controls speed throttling", true, Setting::Save::NO)
	, fullSpeedLoadingSetting(
		commandController, "fullspeedwhenloading",
		"sets openMSX to full speed when the MSX is loading", false)
{
	throttleSetting        .attach(*this);
	fullSpeedLoadingSetting.attach(*this);
}

ThrottleManager::~ThrottleManager()
{
	throttleSetting        .detach(*this);
	fullSpeedLoadingSetting.detach(*this);
}

void ThrottleManager::updateStatus()
{
	bool newThrottle = throttleSetting.getBoolean() &&
	                   (!loading || !fullSpeedLoadingSetting.getBoolean());
	if (throttle != newThrottle) {
		throttle = newThrottle;
		notify();
	}
}

void ThrottleManager::indicateLoadingState(bool state)
{
	if (state) {
		++loading;
	} else {
		--loading;
	}
	assert(loading >= 0);
	updateStatus();
}

void ThrottleManager::update(const Setting& /*setting*/) noexcept
{
	updateStatus();
}


// class LoadingIndicator:

LoadingIndicator::LoadingIndicator(ThrottleManager& throttleManager_)
	: throttleManager(throttleManager_)
{
}

LoadingIndicator::~LoadingIndicator()
{
	update(false);
}

void LoadingIndicator::update(bool newState)
{
	if (isLoading != newState) {
		isLoading = newState;
		throttleManager.indicateLoadingState(isLoading);
	}
}

} // namespace openmsx
