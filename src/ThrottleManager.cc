#include "ThrottleManager.hh"

#include "BooleanSetting.hh"


namespace openmsx {

ThrottleManager::ThrottleManager(BooleanSetting& throttleSetting_, BooleanSetting& fullSpeedLoadingSetting_)
	: throttleSetting(throttleSetting_)
	, fullSpeedLoadingSetting(fullSpeedLoadingSetting_)
	, loading(0)
{
}

ThrottleManager::~ThrottleManager()
{
}

bool ThrottleManager::isThrottled() const
{
	return (throttleSetting.getValue()) && (fullSpeedLoadingSetting.getValue() ? (loading == 0) : true);
}

void ThrottleManager::indicateLoadingState(bool state)
{
	if (state) {
		++loading;
	} else { 
		--loading;
	}
	assert (loading >= 0);
}

}
