// $Id$

#ifndef __KEYJOYSTICK_HH__
#define __KEYJOYSTICK_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "Keys.hh"
#include "MSXConfig.hh"
#include <SDL/SDL.h>

namespace openmsx {

class KeyJoystick : public JoystickDevice, EventListener
{
public:
	KeyJoystick();
	virtual ~KeyJoystick();

	// Pluggable
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void plug(Connector* connector, const EmuTime& time) throw ();
	virtual void unplug(const EmuTime& time);

	// KeyJoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	// EventListener
	virtual bool signalEvent(const SDL_Event& event) throw();

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

#endif
