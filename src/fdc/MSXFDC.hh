#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXDevice.hh"
#include "MSXMemDevice.hh"
#include "MSXRom16KB.hh"

// This is the interface for the emulated MSX towards the FDC
// in a first stage it will be only on memmorymappedFDC as in
// the Philips NMSxxxx MSX2 machines
//
// The actual FDC is implemented elsewhere and uses backends 
// to talk to actual disk(image)s

// forward declarations
class EmuTime;
class FDC;


class MSXFDC :  virtual public MSXRom16KB
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
		
		//void SaveStateMSX(ofstream savestream);
		
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);  
		byte* getReadCacheLine(word start);
		byte* getWriteCacheLine(word start);
	private:
		FDC* controller;
		bool brokenFDCread;
		byte* emptyRom;
};
#endif
