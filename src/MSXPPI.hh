// $Id$

#ifndef __MSXPPI_HH__
#define __MSXPPI_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"

// This class implements the PPI
// found on ports A8..AB where 
//A8      R/W    I 8255A/ULA9RA041 PPI Port A Memory PSLOT Register (RAM/ROM)
//			Bit   Expl.
//			0-1   PSLOT number 0-3 for memory at 0000-3FFF
//			2-3   PSLOT number 0-3 for memory at 4000-7FFF
//			4-5   PSLOT number 0-3 for memory at 8000-BFFF
//			6-7   PSLOT number 0-3 for memory at C000-FFFF
//A9      R      I 8255A/ULA9RA041 PPI Port B Keyboard column inputs
//AA      R/W    I 8255A/ULA9RA041 PPI Port C Kbd Row sel,LED,CASo,CASm
//AB      W      I 8255A/ULA9RA041 Mode select and I/O setup of A,B,C

class MSXPPI : public MSXDevice
{	// this class is a singleton class
	// usage: MSXConfig::instance()->method(args);
 
	static MSXDevice *instance();

	private:
		//MSXPPI(); // private constructor -> can only construct self
		static MSXPPI *volatile oneInstance; 
		void Keyboard(void);
		void KeyGhosting(void);
		int real_MSX_keyboard;
		byte MSXKeyMatrix[16];
		byte port_a9;
		byte port_aa;
		byte port_ab;
	public:
		MSXPPI(); 
		~MSXPPI(); 
		// don't forget you inherited from MSXDevice
		void init();
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);

        friend class Inputs;
};
#endif
