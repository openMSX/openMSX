// $Id$

#ifndef FILEOPERATIONS_HH
#define FILEOPERATIONS_HH

#include "unistdp.hh" // needed for mode_t definition when building with VC++
#include <string>
#include <sys/types.h>
#include <fstream>

namespace openmsx {

namespace FileOperations {

	const char nativePathSeparator =
#ifdef _WIN32
		'\\';
#else
		'/';
#endif

	/**
	 * Expand the '~' character to the users home directory
	 * @param path Pathname, with or without '~' character
	 * @result The expanded pathname
	 */
	std::string expandTilde(const std::string& path);

	/**
	 * Create the specified directory. Does some sanity checks so that
	 * it bahaves the same on all platforms. The mode parameter is ignored
	 * on windows. For compatibility with *nix creating the root dir (or a
	 * drivename) is not an error instead the operation is silently
	 * ignored. This function can only create one dircetory at-a-time. You
	 * probably want to use the mkdirp function (see below).
	 * @param path The path of the directory to create
	 * @param mode The permission bits (*nix only)
	 * @throw FileException
	 */
	void mkdir(const std::string& path, mode_t mode);

	/**
	 * Acts like the unix command "mkdir -p". Creates the
	 * specified directory, including the parent directories.
	 * @param path The path of the directory to create
	 * @throw FileException
	 */
	void mkdirp(const std::string& path);

	/**
	 * Call unlink() in a platform-independent manner
	 */
	int unlink(const std::string& path);

	/**
	 * Call rmdir() in a platform-independent manner
	 */
	int rmdir(const std::string& path);

	/**
	 * Call stat() and return the mode field from the stat structure
	 * @param filename the file path
	 * @param mode the mode field to be returned
	 * @result The return value from stat()
	 */
	int statGetMode(const std::string& filename, mode_t& mode);

	/**
	 * Call fopen() in a platform-independent manner
	 * @param filename the file path
	 * @param mode the mode parameter, same as fopen
	 * @result A pointer to the opened file, or NULL on error
	 */
	FILE* openFile(const std::string& filename, const std::string& mode);

	/**
	 * Open an ofstream in a platform-independent manner
	 * @param stream an ofstream
	 * @param filename the file path
	 */
	void openofstream(std::ofstream& stream, const std::string& filename);

	/**
	 * Open an ofstream in a platform-independent manner
	 * @param stream an ofstream
	 * @param filename the file path
	 * @param mode the open mode
	 */
	void openofstream(std::ofstream& stream, const std::string& filename,
		std::ios_base::openmode mode);

	/**
	 * Returns the file portion of a path name.
	 * @param path The pathname
	 * @result The file portion
	 */
	std::string getFilename(const std::string& path);

	/**
	 * Returns the directory portion of a path.
	 * @param path The pathname
	 * @result The directory portion. This includes the ending '/'.
	 *         If path doesn't have a directory portion the result
	 *         is an empty string.
	 */
	std::string getBaseName(const std::string& path);

	/**
	 * Returns the extension portion of a path.
	 * @param path The pathname
	 * @result The extension portion. This excludes the '.'.
	 *         If path doesn't have an extension portion the result
	 *         is an empty string.
	 */
	std::string getExtension(const std::string& path);

	/** Join two paths.
	 * Returns the equivalent of 'path1 + '/' + path2'. If 'part2' is an
	 * absolute path, that path is returned ('part1' is ignored). If
	 * 'part1' is empty or if it already ends with '/', there will be no
	 * extra '/' added inbetween 'part1' and 'part2'.
	 */
	std::string join(const std::string& part1, const std::string& part2);
	std::string join(const std::string& part1, const std::string& part2,
	                 const std::string& part3);
	std::string join(const std::string& part1, const std::string& part2,
	                 const std::string& part3, const std::string& part4);

	/**
	 * Returns the path in conventional path-delimiter.
	 * @param path The pathname.
	 * @result The path in conventional path-delimiter.
	 *    On UNI*Y systems, it will have no effect indeed.
	 *    Just for portability issue. (Especially for Win32)
	 */
	std::string getConventionalPath(const std::string& path);

	/**
	 * Returns the path in native path-delimiter.
	 * @param path The pathname.
	 * @result The path in native path-delimiter.
	 *    On UNI*Y systems, it will have no effect indeed.
	 *    Just for portability issue. (Especially for Win32)
	 */
	std::string getNativePath(const std::string& path);

	/** Returns the current working directory.
	 * @throw FileException (for example when directory has been deleted).
	 */
	std::string getCurrentWorkingDirectory();

	/** Transform given path into an absolute path
	 * @throw FileException
	 */
	std::string getAbsolutePath(const std::string& path);

	/**
	 * Checks whether it's a absolute path or not.
	 * @param path The pathname.
	 */
	bool isAbsolutePath(const std::string& path);

	/**
	 * Get user's home directory.
	 * @param username The name of the user
	 * @result Home directory of the user or empty string in case of error
	 * UNI*Y: get from env var "HOME" or from /etc/passwd
	 *        empty string means current user
	 * Win32: Currently use "My Documents" as home directory.
	 *        Not "Documents and Settings".
	 *        This is because to support Win9x.
	 *        Ignores the username parameter
	 */
	std::string getUserHomeDir(const std::string& username = "");

	/**
	 * Get the openMSX dir in the user's home directory.
	 * Default value is "~/.openMSX" (UNIX) or "~/openMSX" (win)
	 */
	const std::string& getUserOpenMSXDir();

	/**
	 * Get the openMSX data dir in the user's home directory.
	 * Default value is "~/.openMSX/share" (UNIX) or "~/openMSX/share" (win)
	 */
	std::string getUserDataDir();

	/**
	 * Get system directory.
	 * UNI*Y: statically defined as "/opt/openMSX/share".
	 * Win32: use "same directory as .exe" + "/share".
	 */
	std::string getSystemDataDir();

	/**
	* Get the current directory of the specified drive
	* Linux: just return an empty string
	*/
	std::string expandCurrentDirFromDrive(const std::string& path);

	/**
	 * Is this a regular file (no directory, device, ..)?
	 */
	bool isRegularFile(const std::string& filename);

	/**
	 * Is this a directory?
	 */
	bool isDirectory(const std::string& directory);

	/**
	 * Does this file (directory) exists?
	 */
	bool exists(const std::string& filename);

	/** Get the date/time of last modification
	 * @throws FileException
	 */
	time_t getModificationDate(const std::string& filename);

	/**
	 * Gets the next numbered file name with the specified prefix in the
	 * specified directory, with the specified extension. Examples:
	 * automatic numbering of filenames for new screenshots or sound logs.
	 * @param directory Name of the directory in the openMSX user dir in
	 * which should be searched for the next filename
	 * @param prefix Prefix of the filename with numbers
	 * @param extension Extension of the filename with numbers
	 */
	std::string getNextNumberedFileName(
		const std::string& directory, const std::string& prefix,
		const std::string& extension);

	/** Helper function for parsing filename arguments in Tcl commands.
	 * - If argument is empty then getNextNumberedFileName() is used
	 *   with given directory, prefix and extension.
	 * - If argument doesn't already end with the given extension that
	 *   extension is appended.
	 * - If argument doesn't already include a directory, the given
	 *   directory is used (and created if required).
	 */
	std::string parseCommandFileArgument(
		const std::string& argument, const std::string& directory,
		const std::string& prefix,   const std::string& extension);

	/**
	 * Get the name of the temp directory on the system.
	 * Typically /tmp on *nix and C:/WINDOWS/TEMP on windows
	 */
	std::string getTempDir();

	/**
	 * Open a new file with a unique name in the provided directory
	 * @param directory directory in which to open the temp file
	 * @param filename [output param] the name of the resulting file
	 * @result pointer to the opened file
	 */
	FILE* openUniqueFile(const std::string& directory, std::string& filename);

} // namespace FileOperations

} // namespace openmsx

#endif
