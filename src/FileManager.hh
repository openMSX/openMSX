// $Id$
//
// FileManager

#ifndef __FILEMANAGER_HH__
#define __FILEMANAGER_HH__

#include <fstream>
#include <string>
#include <list>

#include "config.h"

#include "openmsx.hh"
#include "MSXConfig.hh"
#include "MSXFilePath.hh"

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


class FileManagerException : public MSXException {
	public:
		FileManagerException(const std::string &desc) : MSXException(desc) {}
};

/**
 * Manage all Files access, both local and remote
 * there are 2 ways: cached and non-cached
 * only RO or PreferedRW files can be opened remote,
 * and thus can be cached
 */
class FileManager
{
	public:
		/**
		 * The One FileManager instance
		 */
		FileManager* instance();
		
		~FileManager();
		
		/**
		 * open a ROM (readonly of course!)
		 */
		IFILETYPE* openRom(std::string& filename);

		/**
		 * open a DISK (with respect to file perms
		 * and protocol)
		 */
		IFILETYPE* openDisk(std::string& filename);

		/**
		 * open a DISK (rw, if not, fail)
		 *
		 */
		 IFILETYPE* openDiskRW(std::string& filename);

		/**
		 * open a state file [SRAM, ...]
		 * has to be rw, stored at the apropriate
		 * user-specific place
		 */
		 IFILETYPE* openState(std::string& filename);

		/**
		 * open a config file [.xml]
		 * searched at the apropriate places
		 */
		 IFILETYPE* openConfig(std::string& filename);

	private:

		// predefine
		class Path;

		FileManager();

		/// The one instance
		static FileManager* _instance;

		// filepath vars
		std::list<Path*> path_list;
		std::list<Path*> path_list_local_only;

		// filecaching vars
		std::string cachedir;

		/// Private Path class for FileManager
		class Path
		{
			public:
				Path(const std::string &path);
				~Path();

				const std::string path;

				/**
				 * Is path an HTTP path?
				 */
				bool isHTTP();
				/**
				 * Is path an FTP path?
				 */
				bool isFTP();
		};

		MSXConfig::FilePath* filepath;
};

#endif // __FILEMANAGER_HH__
