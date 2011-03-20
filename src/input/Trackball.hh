// $Id$

#ifndef TRACKBALL_HH
#define TRACKBALL_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class Trackball : public JoystickDevice, private MSXEventListener
                , private StateChangeListener
{
public:
	Trackball(MSXEventDistributor& eventDistributor,
	          StateChangeDistributor& stateChangeDistributor);
	virtual ~Trackball();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createTrackballStateChange(EmuTime::param time,
		int deltaX, int deltaY, byte press, byte release);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// JoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(shared_ptr<StateChange> event);
	virtual void stopReplay(EmuTime::param time);

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	signed char deltaX, deltaY;
	byte lastValue;
	byte status;
};

} // namespace openmsx

#endif
