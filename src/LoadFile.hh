// $Id$
//
// LoadFile mixin

#ifndef __LOADFILE_HH__
#define __LOADFILE_HH__

#include "config.h"
#include "openmsx.hh"
#include "msxconfig.hh"

#ifdef HAVE_FSTREAM_TEMPL
#define FILETYPE std::ifstream<byte>
#else
#define FILETYPE std::ifstream
#endif

class LoadFile
{
	public:
		/**
		 * Load a file of given size and allocates memory for it, 
		 * the pointer memorybank will point to this memory block.
		 * The filename is the "filename" parameter in config file.
		 * The first "skip_headerbytes" bytes of the file are ignored.
		 */
		void loadFile(byte** memoryBank, int fileSize);
		int  loadFile(byte** memoryBank);

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

		FILETYPE* openFile();
		void readFile(FILETYPE* file, int fileSize, byte** memoryBank);
		
		/**
		 * patch a file after it has being loaded in the memory bank
		 * if configured in configfile
		 */
		void patchFile(byte* memoryBank, int size);
};


#endif // __LOADFILE_HH__
