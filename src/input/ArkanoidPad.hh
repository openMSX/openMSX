// $Id$

#ifndef ARKANOIDPAD_HH
#define ARKANOIDPAD_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class ArkanoidPad : public JoystickDevice, private MSXEventListener
                  , private StateChangeListener
{
public:
	explicit ArkanoidPad(MSXEventDistributor& eventDistributor,
	                     StateChangeDistributor& stateChangeDistributor);
	virtual ~ArkanoidPad();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Pluggable
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// JoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	// MSXEventListener
	virtual void signalEvent(const shared_ptr<const Event>& event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(const shared_ptr<StateChange>& event);
	virtual void stopReplay(EmuTime::param time);

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	int shiftreg;
	int dialpos;
	byte buttonStatus;
	byte lastValue;
};
SERIALIZE_CLASS_VERSION(ArkanoidPad, 2);

} // namespace openmsx

#endif
