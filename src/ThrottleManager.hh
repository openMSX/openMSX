#ifndef THROTTLEMANAGER_HH
#define THROTTLEMANAGER_HH

namespace openmsx {

class BooleanSetting;
	
/**
 * Manages the throttle state of openMSX. It depends on the throttle setting,
 * but also on the fullspeedwhenfastloading setting and if the MSX has
 * notified us that it is loading...
 * TODO: make this class a Subject, so that we can Observe it.
 */
class ThrottleManager
{
public:
	ThrottleManager(BooleanSetting& throttleSetting_, BooleanSetting& fullSpeedLoadingSetting_);
	~ThrottleManager();

	/**
	 * Ask if throttling is enabled. Depends on the throttle setting, but
	 * also on the fullspeedwhenfastloading setting and if the MSX has
	 * notified us that it is loading... To be used for the timing.
	 */
	bool isThrottled() const;

	/**
	 * Use to indicate that the MSX is in a loading state, so that full
	 * speed can be enabled. Note that the caller can only call it once,
	 * when the state changes. It may not call it twice in a row with the
	 * same value for the argument.
	 * @param state true for loading, false for not loading
	 */
	void indicateLoadingState(bool state);

private:

        BooleanSetting& throttleSetting;
	BooleanSetting& fullSpeedLoadingSetting;
	int loading;
};

}

#endif
