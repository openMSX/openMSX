#ifndef __MSXPPI_H__
#define __MSXPPI_H__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"

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
	static byte Keys[336][2];

	private:
		MSXPPI(); // private constructor -> can only construct self
		static MSXPPI *volatile oneInstance; 
		void Keyboard(void);
		byte MSXKeyMatrix[16];
	public:
		~MSXPPI(); 
		// don't forget you inherited from MSXDevice
		void InitMSX();
		//void ResetMSX();
		//void StopMSX();
		//void RestoreMSX();
		//void SaveStateMSX(ofstream savestream);
		byte readIO(byte port,UINT64 TStates);
		void writeIO(byte port,byte value,UINT64 TStates);
};
#endif
