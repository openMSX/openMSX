// $Id$

#ifndef __FILEOPERATIONS_HH__
#define __FILEOPERATIONS_HH__

#include <string>

using namespace std;


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
};

#endif
