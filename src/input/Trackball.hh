#ifndef TRACKBALL_HH
#define TRACKBALL_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class Trackball final : public JoystickDevice, private MSXEventListener
                      , private StateChangeListener
{
public:
	Trackball(MSXEventDistributor& eventDistributor,
	          StateChangeDistributor& stateChangeDistributor);
	~Trackball() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createTrackballStateChange(EmuTime::param time,
		int deltaX, int deltaY, byte press, byte release);

	void syncCurrentWithTarget(EmuTime::param time);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	// MSXEventListener
	void signalMSXEvent(const std::shared_ptr<const Event>& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	EmuTime lastSync; // last time we synced current with target
	signed char targetDeltaX, targetDeltaY; // immediately follows host events
	signed char currentDeltaX, currentDeltaY; // follows targetXY with some delay
	byte lastValue;
	byte status;
	bool smooth; // always true, except for bw-compat savestates
};
SERIALIZE_CLASS_VERSION(Trackball, 2);

} // namespace openmsx

#endif
