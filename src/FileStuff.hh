// $Id$
//
// FileStuff mixin

#ifndef __FILESTUFF_HH__
#define __FILESTUFF_HH__

#include "config.h"
#include "openmsx.hh"
#include "msxconfig.hh"

#ifdef HAVE_FSTREAM_TEMPL
#define IFILETYPE std::ifstream<byte>
#else
#define IFILETYPE std::ifstream
#endif

#ifdef HAVE_FSTREAM_TEMPL
#define OFILETYPE std::ofstream<byte>
#else
#define OFILETYPE std::ofstream
#endif

#ifdef HAVE_FSTREAM_TEMPL
#define IOFILETYPE std::fstream<byte>
#else
#define IOFILETYPE std::fstream
#endif

class FileStuff
{
	public:
		/**
		 * try to find a readable file in the current rompath with matching filename
		 * returns the filename with path as string
		 */
		 std::string  findFileName(std::string filename);
		/**
		 * Open a file for reading only.
		 */
		IFILETYPE* openFileRO(std::string filename);
		/**
		 * Open a file for reading and writing.
		 * if not writeable then fail
		 */
		IOFILETYPE* openFileMustRW(std::string filename);
		/**
		 * Open a file for reading and writing.
		 * if not writeable then open readonly
		 */
		IOFILETYPE* openFilePreferRW(std::string filename);

		/** Following are for creating/reusing files **/
		/** if not writeable then fail **/
		IOFILETYPE* openFileAppend(std::string filename);
		IOFILETYPE* openFileTruncate(std::string filename);
	private:
		/**
		 * Objects of this class can not be constructed 
		 */
		FileStuff() {}
};

#endif // __FILESTUFF_HH__
