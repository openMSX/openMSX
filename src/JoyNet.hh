// $Id$

#ifndef __JOYNET_HH__
#define __JOYNET_HH__

#include "JoystickDevice.hh"
#include "MSXException.hh"


class JoyNetException : public MSXException {
	public:
		JoyNetException(const std::string &desc) : MSXException(desc) {}
};

class JoyNet : public JoystickDevice
{
	public:
		JoyNet(int joyNum);
		virtual ~JoyNet();

		//Pluggable
		virtual const std::string &getName();
		
		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

	private:
		static const int JOY_UP      = 0x01;
		static const int JOY_DOWN    = 0x02;
		static const int JOY_LEFT    = 0x04;
		static const int JOY_RIGHT   = 0x08;
		static const int JOY_BUTTONA = 0x10;
		static const int JOY_BUTTONB = 0x20;

		std::string name;

		int joyNum;
		byte status;
};
#endif
