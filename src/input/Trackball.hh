#ifndef TRACKBALL_HH
#define TRACKBALL_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class Trackball final : public JoystickDevice, private MSXEventListener
                      , private StateChangeListener
{
public:
	Trackball(MSXEventDistributor& eventDistributor,
	          StateChangeDistributor& stateChangeDistributor);
	~Trackball();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createTrackballStateChange(EmuTime::param time,
		int deltaX, int deltaY, byte press, byte release);

	// Pluggable
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	signed char deltaX, deltaY;
	byte lastValue;
	byte status;
};

} // namespace openmsx

#endif
