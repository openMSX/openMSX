// $Id$
//
// FileOpener mixin

#ifndef __FILEOPENER_HH__
#define __FILEOPENER_HH__

#include "config.h"
#include "openmsx.hh"
#include "MSXConfig.hh"
#include <fstream>

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


class FileOpenerException : public MSXException {
	public:
		FileOpenerException(const std::string &desc) : MSXException(desc) {}
};

class FileOpener
{
	public:
		/**
		 * try to find a readable file in the current rompath with matching filename
		 * returns the filename with path as string
		 */
		static std::string  findFileName(std::string filename);
		/**
		 * Open a file for reading only.
		 */
		static IFILETYPE* openFileRO(std::string filename);
		/**
		 * Open a file for reading and writing.
		 * if not writeable then fail
		 */
		static IOFILETYPE* openFileMustRW(std::string filename);
		/**
		 * Open a file for reading and writing.
		 * if not writeable then open readonly
		 */
		static IOFILETYPE* openFilePreferRW(std::string filename);

		/** Following are for creating/reusing files **/
		/** if not writeable then fail **/
		static IOFILETYPE* openFileAppend(std::string filename);
		static IOFILETYPE* openFileTruncate(std::string filename);
	private:
		/**
		 * Objects of this class can not be constructed 
		 */
		FileOpener() {}
};

#endif // __FILEOPENER_HH__
