// $Id$

#ifndef __FILEOPERATIONS_HH__
#define __FILEOPERATIONS_HH__

#include <string>


class FileOperations
{
	public:
		/**
		 * Expand the '~' character to the users home directory
		 * @parm path Pathname, with or without '~' character
		 * @result The expanded pathname
		 */
		static std::string expandTilde(const std::string &path);

		/**
		 * Acts like the unix command "mkdir -p". Creates the
		 * specified directory, including the parent directories.
		 * @param path The path of the directory to create
		 * @return True iff successful 
		 */
		static bool mkdirp(const std::string &path);

		/**
		 * Returns the file portion of a path name.
		 * @param path The pathname
		 * @result The file portion
		 */
		static std::string getFilename(const std::string &path);

		/**
		 * Returns the directory portion of a path.
		 * @param path The pathname
		 * @result The directory portion
		 */
		static std::string getBaseName(const std::string &path);
};

#endif
