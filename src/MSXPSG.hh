// $Id$

#ifndef __MSXPSG_HH__
#define __MSXPSG_HH__

#include "MSXDevice.hh"
#include "emutime.hh"
#include "AY8910.hh"
#include "JoystickPorts.hh"

class MSXPSG : public MSXDevice, AY8910Interface
{
	// MSXDevice
	public:
		MSXPSG();
		~MSXPSG(); 
		
		void init();
		void reset();
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
	private:
		int registerLatch;
		AY8910 *ay8910;
	
	// AY8910Interface
	public:
		byte readA();
		byte readB();
		void writeA(byte value);
		void writeB(byte value);

	private:
		JoystickPorts *joyPorts;
};
#endif
