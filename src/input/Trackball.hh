#ifndef TRACKBALL_HH
#define TRACKBALL_HH

#include "EmuTime.hh"
#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"
#include <cstdint>

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
	void createTrackballStateChange(EmuTime time,
		int deltaX, int deltaY, uint8_t press, uint8_t release);

	void syncCurrentWithTarget(EmuTime time);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	EmuTime lastSync = EmuTime::zero(); // last time we synced current with target
	int8_t targetDeltaX = 0, targetDeltaY = 0; // immediately follows host events
	int8_t currentDeltaX = 0, currentDeltaY = 0; // follows targetXY with some delay
	uint8_t lastValue = 0;
	uint8_t status = JOY_BUTTONA | JOY_BUTTONB;
	bool smooth = true; // always true, except for bw-compat savestates
};
SERIALIZE_CLASS_VERSION(Trackball, 2);

} // namespace openmsx

#endif
