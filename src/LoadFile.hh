// $Id$
//
// LoadFile mixin

#ifndef __LOADFILE_HH__
#define __LOADFILE_HH__

#include "config.h"
#include "openmsx.hh"
#include "MSXConfig.hh"
#include "FileOpener.hh"

class LoadFile
{
	public:
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

	protected:
		/**
		 * pure virtual function used to access the device's
		 * config object.
		 */
		virtual MSXConfig::Device* getDeviceConfig() = 0;

		/**
		 * Objects of this class can only be constructed by
		 * inherited classes.
		 */
		LoadFile() {}

	private:
		/** block usage */
		LoadFile(const LoadFile &foo);
		/** block usage */
		LoadFile &operator=(const LoadFile &foo);

		IFILETYPE* openFile();
		void readFile(IFILETYPE* file, int fileSize, byte** memoryBank);
		
		/**
		 * patch a file after it has being loaded in the memory bank
		 * if configured in configfile
		 */
		void patchFile(byte* memoryBank, int size);
};


#endif // __LOADFILE_HH__
