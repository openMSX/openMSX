// $Id$

#ifndef __FILEOPERATIONS_HH__
#define __FILEOPERATIONS_HH__

#include <string>


class FileOperations
{
	public:
		/**
		 *
		 */
		static std::string expandTilde(const std::string &path);

		/**
		 *
		 */
		static bool mkdirp(const std::string &path);
};

#endif
