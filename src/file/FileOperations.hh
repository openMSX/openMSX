// $Id$

#ifndef __FILEOPERATIONS_HH__
#define __FILEOPERATIONS_HH__

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#if	defined(__WIN32__)
#define	mode_t	unsigned short int
#endif

using std::string;


namespace openmsx {

class FileOperations
{
	public:
		/**
		 * Expand the '~' character to the users home directory
		 * @parm path Pathname, with or without '~' character
		 * @result The expanded pathname
		 */
		static string expandTilde(const string &path);

		/**
		 * Acts like the unix command "mkdir -p". Creates the
		 * specified directory, including the parent directories.
		 * @param path The path of the directory to create
		 * @return True iff successful 
		 */
		static bool mkdirp(const string &path);

		/**
		 * Returns the file portion of a path name.
		 * @param path The pathname
		 * @result The file portion
		 */
		static string getFilename(const string &path);

		/**
		 * Returns the directory portion of a path.
		 * @param path The pathname
		 * @result The directory portion. This includes the ending '/'.
		 *         If path doesn't has a directory portion the result
		 *         is an empty string.
		 */
		static string getBaseName(const string &path);

		/**
		 * Returns the path in conventional path-delimiter.
		 * @param path The pathname.
		 * @result The path in conventional path-delimiter.
		 * 	On UNI*Y systems, it will have no effect indeed.
		 * 	Just for portability issue. (Especially for Win32)
		 */
		static string getConventionalPath(const string &path);

		/**
		 * Returns the path in native path-delimiter.
		 * @param path The pathname.
		 * @result The path in native path-delimiter.
		 * 	On UNI*Y systems, it will have no effect indeed.
		 * 	Just for portability issue. (Especially for Win32)
		 */
		static string getNativePath(const string &path);

		/**
		 * A wrapper for mkdir().
		 * On some systems, mkdir() does not take permision in
		 * arguments. For such systems, in this function,
		 * adjust arguments.
		 */
		static int doMkdir(const char *name, mode_t mode);

		/**
		 * Checks whether it's a absolute path or not.
		 * @param path The pathname.
		 * @result 1 when absolute path. 0 when relative path.
		 */
		static int FileOperations::isAbsolutePath(string path);

		/**
		 * Set user's home directory.
		 * UNI*Y: get from env-val: "HOME".
		 * Win32: Currently use "My Documents" as home directory.
		 *        Not "Documents and Settings".
		 *        This is because to support Win9x.
		 * @result 0 on success. -1 on any error.
		 */
		static int setUsrDir(void);

		/**
		 * Set system directory.
		 * UNI*Y: statically defined as "/opt/openMSX/".
		 * Win32: use "same directory as .exe" instead.
		 * @result 0 on success. -1 on any error.
		 */
		static int setSysDir(void);

};

} // namespace openmsx

#endif
