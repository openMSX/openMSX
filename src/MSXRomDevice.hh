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

class MSXRomDevice: virtual public MSXDevice
{
	public:
		MSXRomDevice(MSXConfig::Device *config, const EmuTime &time);
		MSXRomDevice(MSXConfig::Device *config, const EmuTime &time, int fileSize);

		virtual ~MSXRomDevice();

	protected:
		/**
		 * Byte region used by inherited classes
		 * to store the ROM's memory bank.
		 */
		byte* romBank;
		int romSize;

	private:
		/**
		 * create and register RomPatchInterfaces
		 * [called from constructor]
		 * for each patchcode parameter, construct apropriate patch
		 * object and register it at the CPU
		 */
		void handleRomPatchInterfaces();
		
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
		void loadFile(int fileSize);
		void loadFile();

		IFILETYPE* openFile();
		void readFile(IFILETYPE* file, int fileSize);
		
		/**
		 * patch a file after it has being loaded in the memory bank
		 * if configured in configfile
		 */
		void patchFile();
		
		std::list<MSXRomPatchInterface*> romPatchInterfaces;
};

#endif // __MSXROM_HH__
