#ifndef THROTTLEMANAGER_HH
#define THROTTLEMANAGER_HH

#include "Subject.hh"
#include "BooleanSetting.hh"

namespace openmsx {

class CommandController;

/**
 * Manages the throttle state of openMSX. It depends on the throttle setting,
 * but also on the fullspeedwhenfastloading setting and if the MSX has
 * notified us that it is loading... If you want to know about the throttle
 * status of openMSX, attach to me! (And not just to throttleSetting!)
 */
class ThrottleManager final : public Subject<ThrottleManager>
                            , private Observer<Setting>
{
public:
	explicit ThrottleManager(CommandController& commandController);
	~ThrottleManager();

	/**
	 * Ask if throttling is enabled. Depends on the throttle setting, but
	 * also on the fullspeedwhenfastloading setting and if the MSX has
	 * notified us that it is loading... To be used for the timing.
	 */
	[[nodiscard]] bool isThrottled() const { return throttle; }

	[[nodiscard]] auto& getFullSpeedLoadingSetting() { return fullSpeedLoadingSetting; }

private:
	friend class LoadingIndicator;

	/**
	 * Use to indicate that the MSX is in a loading state, so that full
	 * speed can be enabled. Note that the caller can only call it once,
	 * when the state changes. It may not call it twice in a row with the
	 * same value for the argument.
	 * @param state true for loading, false for not loading
	 */
	void indicateLoadingState(bool state);

	void updateStatus();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	BooleanSetting throttleSetting;
	BooleanSetting fullSpeedLoadingSetting;
	int loading = 0;
	bool throttle = true;
};

/**
 * Used by a device to indicate when it is loading.
 */
class LoadingIndicator
{
public:
	explicit LoadingIndicator(ThrottleManager& throttleManager);
	~LoadingIndicator();

	/**
	 * Called by the device to indicate its loading state may have changed.
	 */
	void update(bool newState);

private:
	ThrottleManager& throttleManager;
	bool isLoading = false;
};

} // namespace openmsx

#endif
