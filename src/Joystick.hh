// $Id$

#ifndef __JOYSTICK_HH__
#define __JOYSTICK_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "MSXException.hh"
#include <SDL/SDL.h>


class JoystickException : public MSXException {
	public:
		JoystickException(const std::string &desc) : MSXException(desc) {}
};

class Joystick : public JoystickDevice, EventListener
{
	public:
		Joystick(int joyNum);
		virtual ~Joystick();

		//Pluggable
		virtual const std::string &getName() const;
		
		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

		//EventListener
		virtual bool signalEvent(SDL_Event &event, const EmuTime &time);

	private:
		static const int THRESHOLD = 32768/10;

		std::string name;

		int joyNum;
		SDL_Joystick* joystick;
		byte status;
};
#endif
