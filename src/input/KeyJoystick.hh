// $Id$

#ifndef __KEYJOYSTICK_HH__
#define __KEYJOYSTICK_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "Keys.hh"
#include "MSXConfig.hh"

namespace openmsx {

class KeyJoystick : public JoystickDevice, private EventListener
{
public:
	KeyJoystick();
	virtual ~KeyJoystick();

	// Pluggable
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// KeyJoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	// EventListener
	virtual bool signalEvent(const Event& event);

private:
	Keys::KeyCode getConfigKeyCode(const string& keyname, const Config* config);

	byte status;

	Keys::KeyCode upKey;
	Keys::KeyCode rightKey;
	Keys::KeyCode downKey;
	Keys::KeyCode leftKey;
	Keys::KeyCode buttonAKey;
	Keys::KeyCode buttonBKey;
};

} // namespace openmsx

#endif // __KEYJOYSTICK_HH__
