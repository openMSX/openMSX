// $Id$

// This class implements the PPI (8255)
//
//   PPI    MSX-I/O  Direction  MSX-Function
//  PortA    0xA8      Out     Memory primary slot register
//  PortB    0xA9      In      Keyboard column inputs
//  PortC    0xAA      Out     Keyboard row select / CAPS / CASo / CASm / SND
//  Control  0xAB     In/Out   Mode select for PPI
//
//  Direction indicates the direction normally used on MSX.
//  Reading from an output port returns the last written byte.
//  Writing to an input port has no immediate effect.
//
//  PortA combined with upper half of PortC form groupA
//  PortB               lower                    groupB
//  GroupA can be in programmed in 3 modes
//   - basic input/output
//   - strobed input/output
//   - bidirectional
//  GroupB can only use the first two modes.
//  Only the first mode is used on MSX, only this mode is implemented yet.
//
//  for more detail see
//    http://w3.qahwah.net/joost/openMSX/8255.pdf

#ifndef __MSXPPI_HH__
#define __MSXPPI_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"
#include "I8255.hh"
#include "Keyboard.hh"
#include "KeyClick.hh"
#include "MSXCassettePort.hh"

// David Hermans original comments 
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


class MSXPPI : public MSXDevice, I8255Interface
{
	// MSXDevice
	public:
		/**
		 * Constructor.
		 * This is a singleton class. Constructor can only be used once
		 * by the class devicefactory.
		 */
		MSXPPI(MSXConfig::Device *config); 

		/**
		 * Destructor
		 */
		~MSXPPI(); 

		/**
		 * This is a singleton class. This method returns a reference 
		 * to the single instance of this class.
		 */
		static MSXPPI *instance();
		
		void init();
		void reset();
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
	private:
		static MSXPPI *oneInstance;
		I8255 *i8255;
	
	// I8255Interface
	public:
		byte readA(const Emutime &time);
		byte readB(const Emutime &time);
		nibble readC0(const Emutime &time);
		nibble readC1(const Emutime &time);
		void writeA(byte value, const Emutime &time);
		void writeB(byte value, const Emutime &time);
		void writeC0(nibble value, const Emutime &time);
		void writeC1(nibble value, const Emutime &time);
	
	private:
		Keyboard *keyboard;
		byte MSXKeyMatrix[Keyboard::NR_KEYROWS];
		int selectedRow;
		KeyClick *click;
		CassettePortInterface *cassette;
};
#endif
