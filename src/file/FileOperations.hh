// $Id$

#ifndef FILEOPERATIONS_HH
#define FILEOPERATIONS_HH

#include <string>

namespace openmsx {

class FileOperations
{
public:
	/**
	 * Expand the '~' character to the users home directory
	 * @param path Pathname, with or without '~' character
	 * @result The expanded pathname
	 */
	static std::string expandTilde(const std::string& path);

	/**
	 * Acts like the unix command "mkdir -p". Creates the
	 * specified directory, including the parent directories.
	 * @param path The path of the directory to create
	 * @return True iff successful 
	 * @throw FileException
	 */
	static void mkdirp(const std::string& path);

	/**
	 * Returns the file portion of a path name.
	 * @param path The pathname
	 * @result The file portion
	 */
	static std::string getFilename(const std::string& path);

	/**
	 * Returns the directory portion of a path.
	 * @param path The pathname
	 * @result The directory portion. This includes the ending '/'.
	 *         If path doesn't has a directory portion the result
	 *         is an empty string.
	 */
	static std::string getBaseName(const std::string& path);

	/**
	 * Returns the path in conventional path-delimiter.
	 * @param path The pathname.
	 * @result The path in conventional path-delimiter.
	 * 	On UNI*Y systems, it will have no effect indeed.
	 * 	Just for portability issue. (Especially for Win32)
	 */
	static std::string getConventionalPath(const std::string& path);

	/**
	 * Returns the path in native path-delimiter.
	 * @param path The pathname.
	 * @result The path in native path-delimiter.
	 * 	On UNI*Y systems, it will have no effect indeed.
	 * 	Just for portability issue. (Especially for Win32)
	 */
	static std::string getNativePath(const std::string& path);

	/**
	 * Checks whether it's a absolute path or not.
	 * @param path The pathname.
	 * @result 1 when absolute path. 0 when relative path.
	 */
	static bool isAbsolutePath(const std::string& path);

	/**
	 * Get user's home directory.
	 * UNI*Y: get from env-val: "HOME".
	 * Win32: Currently use "My Documents" as home directory.
	 *        Not "Documents and Settings".
	 *        This is because to support Win9x.
	 */
	static const std::string& getUserHomeDir();

	/**
	 * Get the openMSX dir in the user's home directory.
	 * Default value is "~/.openMSX" (UNIX) or "~/openMSX" (win)
	 */
	static const std::string& getUserOpenMSXDir();

	/**
	 * Get the openMSX data dir in the user's home directory.
	 * Default value is "~/.openMSX/share" (UNIX) or "~/openMSX/share" (win)
	 */
	static std::string getUserDataDir();
	
	/**
	 * Get system directory.
	 * UNI*Y: statically defined as "/opt/openMSX/share".
	 * Win32: use "same directory as .exe" + "/share".
	 */
	static std::string getSystemDataDir();
	
	/**
	* Get the current directory of the specified drive
	* Linux: just return an empty string
	*/
	static std::string expandCurrentDirFromDrive(const std::string& path);

	/**
	 * Is this a regular file (no directory, device, ..)?
	 */
	static bool isRegularFile(const std::string& filename);

	/**
	 * Is this a directory?
	 */
	static bool isDirectory(const std::string& directory);

	/**
	 * Does this file (directory) exists?
	 */
	static bool exists(const std::string& filename);
};

} // namespace openmsx

#endif
