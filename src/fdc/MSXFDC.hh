// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"
#include "MSXIODevice.hh"

// This is the interface for the emulated MSX towards the FDC
// in a first stage it will be only on memmorymappedFDC as in
// the Philips NMSxxxx MSX2 machines
//
// The actual FDC is implemented elsewhere and uses backends 
// to talk to actual disk(image)s

// forward declarations
class FDC;


class MSXFDC : public MSXMemDevice, MSXRomDevice, public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXFDC(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXFDC();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);

		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		FDC* controller;
		bool brokenFDCread;
		byte* emptyRom;
		byte interface;
		byte driveD4;
		byte driveReg;
};
#endif
