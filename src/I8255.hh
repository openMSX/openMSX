// $Id$

// This class implements the Intel 8255 chip
//
// * Only the 8255 is emulated, no surrounding hardware. 
//   Use the class I8255Interface to do that.
// * Only mode 0 (basic input/output) is implemented
// * For techncal details on 8255 see
//    http://w3.qahwah.net/joost/openMSX/8255.pdf

#ifndef __I8255_HH__
#define __I8255_HH__

#include "openmsx.hh"


class I8255Interface
{
	public:
		virtual byte readA()=0;
		virtual byte readB()=0;
		virtual nibble readC0()=0;
		virtual nibble readC1()=0;
		virtual void writeA(byte value)=0;
		virtual void writeB(byte value)=0;
		virtual void writeC0(nibble value)=0;
		virtual void writeC1(nibble value)=0;
};

class I8255
{
	public:
		I8255(I8255Interface &interf); 
		~I8255(); 
		
		void reset();
	
		byte readPortA();
		byte readPortB();
		byte readPortC();
		byte readControlPort();
		void writePortA(byte value);
		void writePortB(byte value);
		void writePortC(byte value);
		void writeControlPort(byte value);
		
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
		
		byte readC0();
		byte readC1();
		void outputPortA(byte value);
		void outputPortB(byte value);
		void outputPortC(byte value);

		int control;
		int latchPortA;
		int latchPortB;
		int latchPortC;

		I8255Interface &interface;
};
#endif
