// $Id$

#ifndef __JOYSTICKDEVICE_HH__
#define __JOYSTICKDEVICE_HH__

#include "openmsx.hh"
#include "Pluggable.hh"


class JoystickDevice : public Pluggable
{
	public:
		/**
		 * Read from the joystick device. The bits in the read byte have
		 * following meaning:
		 *   7    6       5         4         3       2      1     0 
		 * | xx | xx | BUTTON_B | BUTTON_A | RIGHT | LEFT | DOWN | UP |
		 */
		virtual byte read(const EmuTime &time) = 0;

		/**
		 * Write a value to the joystick device. The bits in the written
		 * byte have following meaning:
		 *     7    6    5    4    3     2      1      0
		 *   | xx | xx | xx | xx | xx | pin8 | pin7 | pin6 |
		 * As an optimization, this method might not be called when the
		 * new value is the same as the previous one.
		 */
		virtual void write(byte value, const EmuTime &time) = 0;

		virtual const std::string &getClass() const;
		
	protected:
		static const int JOY_UP      = 0x01;
		static const int JOY_DOWN    = 0x02;
		static const int JOY_LEFT    = 0x04;
		static const int JOY_RIGHT   = 0x08;
		static const int JOY_BUTTONA = 0x10;
		static const int JOY_BUTTONB = 0x20;

};
#endif
