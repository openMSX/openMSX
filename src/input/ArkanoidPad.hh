// $Id$

#ifndef ARKANOIDPAD_HH
#define ARKANOIDPAD_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"

namespace openmsx {

class MSXEventDistributor;

class ArkanoidPad : public JoystickDevice, private MSXEventListener
{
public:
	explicit ArkanoidPad(MSXEventDistributor& eventDistributor);
	virtual ~ArkanoidPad();

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// JoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MSXEventDistributor& eventDistributor;
	int shiftreg;
	int dialpos;
	byte buttonStatus;
	byte lastValue;
};

} // namespace openmsx

#endif
