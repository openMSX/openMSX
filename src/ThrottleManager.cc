#include "ThrottleManager.hh"

#include "BooleanSetting.hh"


namespace openmsx {

ThrottleManager::ThrottleManager(BooleanSetting& throttleSetting_, BooleanSetting& fullSpeedLoadingSetting_)
	: throttleSetting(throttleSetting_)
	, fullSpeedLoadingSetting(fullSpeedLoadingSetting_)
	, loading(0), throttle(true)
{
	throttleSetting.attach(*this);	
	fullSpeedLoadingSetting.attach(*this);	
}

ThrottleManager::~ThrottleManager()
{
	throttleSetting.detach(*this);	
	fullSpeedLoadingSetting.detach(*this);	
}

void ThrottleManager::updateStatus()
{
	const bool throttleOld = throttle;
	throttle = (throttleSetting.getValue()) && (fullSpeedLoadingSetting.getValue() ? (loading == 0) : true);
	if (throttleOld!=throttle) notify();
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

void ThrottleManager::update(const Setting& setting)
{
	updateStatus();
}

}
