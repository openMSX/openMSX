// $Id$

#ifndef __JOYSTICK_HH__
#define __JOYSTICK_HH__

#include "JoystickDevice.hh"
#include "EventDistributor.hh"
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
		virtual const std::string &getName();
		
		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

		//EventListener
		virtual void signalEvent(SDL_Event &event);

	private:
		static const int JOY_UP      = 0x01;
		static const int JOY_DOWN    = 0x02;
		static const int JOY_LEFT    = 0x04;
		static const int JOY_RIGHT   = 0x08;
		static const int JOY_BUTTONA = 0x10;
		static const int JOY_BUTTONB = 0x20;
		static const int THRESHOLD = 32768/10;

		std::string name;

		static bool SDLJoysticksInitialized;
		int joyNum;
		SDL_Joystick* joystick;
		byte status;
};
#endif
