#ifndef SPEEDMANAGER_HH
#define SPEEDMANAGER_HH

#include "Subject.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"

namespace openmsx {

class CommandController;

/**
 * Manages the desired ratio between emutime and real time.
 * Currently this just republishes the value of the "speed" setting.
 */
class SpeedManager final : public Subject<SpeedManager>
                         , private Observer<Setting>
{
public:
	explicit SpeedManager(CommandController& commandController);
	~SpeedManager();

	/**
	 * Return the desired ratio between emutime and real time.
	 */
	[[nodiscard]] double getSpeed() const { return speed; }

private:
	void updateSpeed();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	IntegerSetting speedSetting;
	IntegerSetting fastforwardSpeedSetting;
	BooleanSetting fastforwardSetting;
	double speed = 1.0;
};

} // namespace openmsx

#endif
