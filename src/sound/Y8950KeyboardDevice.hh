// $Id$

#ifndef __Y8950KEYBOARDDEVICE_HH__
#define __Y8950KEYBOARDDEVICE_HH__

#include "openmsx.hh"
#include "Pluggable.hh"


class Y8950KeyboardDevice : public Pluggable
{
	public:
		/**
		 * Send data to the device.
		 * Normally this is used to select a certain row from the
		 * keyboard but you might also connect a non-keyboard device.
		 * A 1-bit means corresponding row is selected ( 0V)
		 *   0-bit                        not selected (+5V) 
		 */
		virtual void write(byte data, const EmuTime &time) = 0;

		/**
		 * Read data from the device.
		 * Normally this are the keys that are pressed but you might
		 * also connect a non-keyboard device.
		 * A 0-bit means corresponding key is pressed 
		 *   1-bit                        not pressed
		 */ 
		virtual byte read(const EmuTime &time) = 0;


		virtual const std::string &getClass();
private:
		static const std::string className;
};

#endif
