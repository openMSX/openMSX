// $Id$

#ifndef __KEYJOYSTICK_HH__
#define __KEYJOYSTICK_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "MSXException.hh"
#include <SDL/SDL.h>


class KeyJoystick : public JoystickDevice, EventListener
{
	public:
		KeyJoystick();
		virtual ~KeyJoystick();

		// Pluggable
		virtual const std::string &getName() const;
		
		// KeyJoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

		// EventListener
		virtual bool signalEvent(SDL_Event &event);

	private:
		byte status;
};

#endif
