// $Id$
//
// Base class for MSX ROM Devices

#ifndef __MSXROM_HH__
#define __MSXROM_HH__

#include "MSXConfig.hh"
#include "MSXDevice.hh"
#include "config.h"
#include "openmsx.hh"
#include "FileOpener.hh"


// forward declaration
class MSXRomPatchInterface;

class MSXRom: virtual public MSXDevice
{
	public:
		MSXRom(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * delete memory bank
		 */
		virtual ~MSXRom();

	protected:
		/**
		 * Load a file into memory.
		 * A memory block is automatically allocated (don't forget to
		 * free it later) the pointer memoryBank will point to this 
		 * memory block. 
		 * The filename is specified in the config file
		 * Optionally a "skip_headerbytes" parameter can be specified in
		 * the config file, in that case the first n bytes of the file
		 * are ignored.
		 * The filesize must be passed as a function parameter.
		 */
		void loadFile(byte** memoryBank, int fileSize);
		
		/**
		 * Load a file into memory.
		 * A memory block is automatically allocated (don't forget to
		 * free it later) the pointer memoryBank will point to this 
		 * memory block. 
		 * The filename is specified in the config file
		 * Optionally a "skip_headerbytes" parameter can be specified in
		 * the config file, in that case the first n bytes of the file
		 * are ignored.
		 * This function will autodetect the filesize. The filesize is
		 * also returned.
		 */
		int loadFile(byte** memoryBank);

		/**
		 * Byte region used by inherited classes
		 * to store the ROM's memory bank.
		 */
		byte* memoryBank;

	private:
		/**
		 * create and register RomPatchInterfaces
		 * [called from constructor]
		 * for each patchcode parameter, construct apropriate patch
		 * object and register it at the CPU
		 */
		void handleRomPatchInterfaces();
		
		/**
		 * patch a file after it has being loaded in the memory bank
		 * if configured in configfile
		 */
		void patchFile(byte* memoryBank, int size);
		
		IFILETYPE* openFile();
		void readFile(IFILETYPE* file, int fileSize, byte** memoryBank);
		
		std::list<MSXRomPatchInterface*> romPatchInterfaces;
};

#endif // __MSXROM_HH__
