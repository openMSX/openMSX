// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXRom.hh"
#include "MSXIODevice.hh"

// This is the interface for the emulated MSX towards the FDC
// in a first stage it will be only on memmorymappedFDC as in
// the Philips NMSxxxx MSX2 machines
//
// The actual FDC is implemented elsewhere and uses backends 
// to talk to actual disk(image)s

// forward declarations
class FDC;


class MSXFDC : public MSXRom, public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXFDC(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXFDC();
		
		void reset(const EmuTime &time);
		
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);  
		byte* getReadCacheLine(word start);

		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
	private:
		FDC* controller;
		bool brokenFDCread;
		byte* emptyRom;
		byte interface;
		byte driveD4;
		byte driveReg;
};
#endif
