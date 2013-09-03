#ifndef TOUCHPAD_HH
#define TOUCHPAD_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class Touchpad : public JoystickDevice, private MSXEventListener
               , private StateChangeListener
{
public:
	Touchpad(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor);
	virtual ~Touchpad();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createTouchpadStateChange(EmuTime::param time,
		byte x, byte y, bool touch, bool button);

	// Pluggable
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// JoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	// MSXEventListener
	virtual void signalEvent(const std::shared_ptr<const Event>& event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(const std::shared_ptr<StateChange>& event);
	virtual void stopReplay(EmuTime::param time);

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	EmuTime start; // last time when CS switched 0->1
	int hostX, hostY; // host state
	byte hostButtons; //
	byte x, y;          // msx state (different from host state
	bool touch, button; //    during replay)
	byte shift; // shift register to both transmit and receive data
	byte channel; // [0..3]   0->x, 3->y, 1,2->not used
	byte last; // last written data, to detect transitions
};

} // namespace openmsx

#endif
