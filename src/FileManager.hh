// $Id$
//
// FileManager

#ifndef __FILEMANAGER_HH__
#define __FILEMANAGER_HH__

#include "config.h"
#include "openmsx.hh"
#include "MSXConfig.hh"
#include <fstream>
#include <string>
#include <list>

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
		 * Open a file for reading only.
		 */
		IFILETYPE* openFileRO(std::string filename, bool cache=false);
		/**
		 * Open a file for reading and writing.
		 * if not writeable then fail
		 */
		IOFILETYPE* openFileMustRW(std::string filename);
		/**
		 * Open a file for reading and writing.
		 * if not writeable then open readonly
		 */
		IOFILETYPE* openFilePreferRW(std::string filename, bool cache=false);

		/** Following are for creating/reusing files **/
		/** if not writeable then fail **/
		IOFILETYPE* openFileAppend(std::string filename);
		IOFILETYPE* openFileTruncate(std::string filename);
	private:

		// predefine
		class Path;

		FileManager();

		/// The one instance
		static FileManager* _instance;

		/**
		 * try to find a readable file in the current rompath with matching filename
		 * returns the filename with path as string, and tells if the original path was an url (http:// or ftp://)
		 * note that it is recommended to put URL's last in the rompaths
		 */
		//std::string findFileName(std::string filename, bool* wasURL=NULL);

		// filepath vars
		std::string separator;
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
};

#endif // __FILEOPENER_HH__
