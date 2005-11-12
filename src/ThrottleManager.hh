#ifndef THROTTLEMANAGER_HH
#define THROTTLEMANAGER_HH

#include "Subject.hh"

namespace openmsx {

class BooleanSetting;
class Setting;
	
/**
 * Manages the throttle state of openMSX. It depends on the throttle setting,
 * but also on the fullspeedwhenfastloading setting and if the MSX has
 * notified us that it is loading... If you want to know about the throttle
 * status of openMSX, attach to me! (And not just to throttleSetting!)
 */
class ThrottleManager : public Subject<ThrottleManager>, public Observer<Setting>
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

	// Observer<Setting>
	void update(const Setting& setting);

private:
	void updateStatus();
        BooleanSetting& throttleSetting;
	BooleanSetting& fullSpeedLoadingSetting;
	int loading;
	bool throttle;
};

}

#endif
