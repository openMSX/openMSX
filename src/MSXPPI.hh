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
//    http://.../8255.pdf    TODO: fix url

#ifndef __MSXPPI_HH__
#define __MSXPPI_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"

// TODO split this class in 
//   - generic 8255 class and
//   - MSX-PPI-Interface class


class MSXPPI : public MSXDevice
{
	public:
		MSXPPI(); 
		~MSXPPI(); 
		static MSXDevice *instance();
		
		void init();
		void reset();
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
	private:
		static MSXPPI *volatile oneInstance; 
	
	private:
		static const int MODE_A   = 0x60;
		static const int MODEA_0  = 0x00;
		static const int MODEA_1  = 0x20;
		static const int MODEA_2  = 0x40;
		static const int MODEA_2_ = 0x60;
		static const int MODE_B  = 0x04;
		static const int MODEB_0 = 0x00;
		static const int MODEB_1 = 0x04;
		static const int DIRECTION_A  = 0x10;
		static const int DIRECTION_B  = 0x02;
		static const int DIRECTION_C0 = 0x01;
		static const int DIRECTION_C1 = 0x08;
		static const int SET_MODE  = 0x80;
		static const int BIT_NR    = 0x0e;
		static const int SET_RESET = 0x01;
		
	
		byte PPIReadPortA();
		byte PPIReadPortB();
		byte PPIReadPortC();
		byte PPIReadControlPort();
		void PPIWritePortA(byte value);
		void PPIWritePortB(byte value);
		void PPIWritePortC(byte value);
		void PPIWriteControlPort(byte value);

		byte PPIReadC0();
		byte PPIReadC1();
		void PPIOutputPortA(byte value);
		void PPIOutputPortB(byte value);
		void PPIOutputPortC(byte value);

		int control;
		int latchPortA;
		int latchPortB;
		int latchPortC;
	
	private:
		byte PPIInterfaceReadA();
		byte PPIInterfaceReadB();
		byte PPIInterfaceReadC0();
		byte PPIInterfaceReadC1();
		void PPIInterfaceWriteA(byte value);
		void PPIInterfaceWriteB(byte value);
		void PPIInterfaceWriteC0(byte value);
		void PPIInterfaceWriteC1(byte value);
	
		void keyGhosting();
		bool keyboardGhosting;
		byte MSXKeyMatrix[16];

		int selectedRow;

        friend class Inputs;
};
#endif
