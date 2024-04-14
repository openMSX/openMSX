#ifndef FILEOPERATIONS_HH
#define FILEOPERATIONS_HH

#include "Date.hh"
#include "unistdp.hh" // needed for mode_t definition when building with VC++
#include "statp.hh"
#include "zstring_view.hh"

#include <fstream>
#include <memory>
#include <optional>
#include <string_view>
#include <sys/types.h>

namespace openmsx::FileOperations {

	struct FClose {
		void operator()(FILE* f) const { fclose(f); }
	};
	using FILE_t = std::unique_ptr<FILE, FClose>;

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
	[[nodiscard]] std::string expandTilde(std::string path);

	/** Returns true iff expandTilde(s) would have an effect.
	  * In other words: expandTilde(s) != s
	  * Mostly useful to assert that tilde expansion has already been done.
	  */
	[[nodiscard]] bool needsTildeExpansion(std::string_view path);

	/**
	 * Create the specified directory. Does some sanity checks so that
	 * it behaves the same on all platforms. The mode parameter is ignored
	 * on windows. For compatibility with *nix creating the root dir (or a
	 * drive name) is not an error instead the operation is silently
	 * ignored. This function can only create one directory at-a-time. You
	 * probably want to use the mkdirp function (see below).
	 * @param path The path of the directory to create
	 * @param mode The permission bits (*nix only)
	 * @throw FileException
	 */
	void mkdir(zstring_view path, mode_t mode);

	/**
	 * Acts like the unix command "mkdir -p". Creates the
	 * specified directory, including the parent directories.
	 * @param path The path of the directory to create
	 * @throw FileException
	 */
	void mkdirp(std::string path);

	/**
	 * Call unlink() in a platform-independent manner
	 */
	int unlink(zstring_view path);

	/**
	 * Call rmdir() in a platform-independent manner
	 */
	int rmdir(zstring_view path);

	/** Recursively delete a file or directory and (in case of a directory)
	  * all its sub-components.
	  */
	int deleteRecursive(zstring_view path);

	/** Call fopen() in a platform-independent manner
	  * @param filename the file path
	  * @param mode the mode parameter, same as fopen
	  * @result A pointer to the opened file, or nullptr on error
	  *         On error the global variable 'errno' is filled in (see
	  *         man fopen for details). */
	[[nodiscard]] FILE_t openFile(zstring_view filename, zstring_view mode);

	/**
	 * Open an ofstream in a platform-independent manner
	 * @param stream an ofstream
	 * @param filename the file path
	 */
	void openOfStream(std::ofstream& stream, zstring_view filename);

	/**
	 * Open an ofstream in a platform-independent manner
	 * @param stream an ofstream
	 * @param filename the file path
	 * @param mode the open mode
	 */
        void openOfStream(std::ofstream& stream, zstring_view filename,
	                  std::ios_base::openmode mode);

        /**
	 * Returns the file portion of a path name.
	 * @param path The pathname
	 * @result The file portion
	 */
	[[nodiscard]] std::string_view getFilename(std::string_view path);

	/**
	 * Returns the directory portion of a path.
	 * @param path The pathname
	 * @result The directory portion. This includes the ending '/'.
	 *         If path doesn't have a directory portion the result
	 *         is an empty string.
	 */
	[[nodiscard]] std::string_view getDirName(std::string_view path);

	/**
	 * Returns the extension portion of a path.
	 * @param path The pathname
	 * @result The extension portion. This excludes the '.'.
	 *         If path doesn't have an extension portion the result
	 *         is an empty string.
	 */
	[[nodiscard]] std::string_view getExtension(std::string_view path);

	/**
	 * Returns the path without extension.
	 * @param path The pathname
	 * @result The path without extension. This excludes the '.'.
	 *         If path doesn't have an extension portion the result
	 *         remains unchanged.
	 */
	[[nodiscard]] std::string_view stripExtension(std::string_view path);

	/** Join two paths.
	 * Returns the equivalent of 'path1 + '/' + path2'. If 'part2' is an
	 * absolute path, that path is returned ('part1' is ignored). If
	 * 'part1' is empty or if it already ends with '/', there will be no
	 * extra '/' added between 'part1' and 'part2'.
	 */
	[[nodiscard]] std::string join(std::string_view part1, std::string_view part2);
	[[nodiscard]] std::string join(std::string_view part1, std::string_view part2, std::string_view part3);
	[[nodiscard]] std::string join(std::string_view part1, std::string_view part2,
	                               std::string_view part3, std::string_view part4);

	/**
	 * Returns the path in conventional path-delimiter.
	 * @param path The pathname.
	 * @result The path in conventional path-delimiter.
	 *    On UNI*Y systems, it will have no effect indeed.
	 *    Just for portability issue. (Especially for Win32)
	 */
#ifdef _WIN32
	[[nodiscard]]              std::string  getConventionalPath(      std::string   path);
#else
	[[nodiscard]] inline const std::string& getConventionalPath(const std::string&  path) { return           path;  }
	[[nodiscard]] inline       std::string  getConventionalPath(      std::string&& path) { return std::move(path); }
#endif

	/**
	 * Returns the path in native path-delimiter.
	 * @param path The pathname.
	 * @result The path in native path-delimiter.
	 *    On UNI*Y systems, it will have no effect indeed.
	 *    Just for portability issue. (Especially for Win32)
	 */
#ifdef _WIN32
	[[nodiscard]]              std::string  getNativePath(      std::string   path);
#else
	[[nodiscard]] inline const std::string& getNativePath(const std::string&  path) { return           path;  }
	[[nodiscard]] inline       std::string  getNativePath(      std::string&& path) { return std::move(path); }
#endif

	/**
	* Get the current directory of the specified drive
	* Linux: return the given string unchanged
	*/
#ifdef _WIN32
	[[nodiscard]]              std::string  expandCurrentDirFromDrive(std::string path);
#else
	[[nodiscard]] inline const std::string& expandCurrentDirFromDrive(const std::string&  path) { return           path;  }
	[[nodiscard]] inline       std::string  expandCurrentDirFromDrive(      std::string&& path) { return std::move(path); }
#endif

	/** Returns the current working directory.
	 * @throw FileException (for example when directory has been deleted).
	 */
	[[nodiscard]] std::string getCurrentWorkingDirectory();

	/** Transform given path into an absolute path
	 * @throw FileException
	 */
	[[nodiscard]] std::string getAbsolutePath(std::string_view path);

	/**
	 * Checks whether it's a absolute path or not.
	 * @param path The pathname.
	 */
	[[nodiscard]] bool isAbsolutePath(std::string_view path);

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
	[[nodiscard]] std::string getUserHomeDir(std::string_view username);

	/**
	 * Get the openMSX dir in the user's home directory.
	 * Default value is "~/.openMSX" (UNIX) or "~/openMSX" (win)
	 */
	[[nodiscard]] const std::string& getUserOpenMSXDir();

	/**
	 * Get the openMSX data dir in the user's home directory.
	 * Default value is "~/.openMSX/share" (UNIX) or "~/openMSX/share" (win)
	 */
	[[nodiscard]] const std::string& getUserDataDir();

	/**
	 * Get system directory.
	 * UNI*Y: statically defined as "/opt/openMSX/share".
	 * Win32: use "same directory as .exe" + "/share".
	 */
	[[nodiscard]] const std::string& getSystemDataDir();

	/**
	 * Get system doc directory.
	 * Win32: use "same directory as .exe" + "/doc".
	 */
	[[nodiscard]] const std::string& getSystemDocDir();

#ifdef _WIN32
	using Stat = struct _stat;
#else
	using Stat = struct stat;
#endif
	/**
	 * Call stat() and return the stat structure
	 * @param filename the file path
	 */
	[[nodiscard]] std::optional<Stat> getStat(zstring_view filename);

	/**
	 * Is this a regular file (no directory, device, ..)?
	 */
	[[nodiscard]] bool isRegularFile(zstring_view filename);
	[[nodiscard]] bool isRegularFile(const Stat& st);

	/**
	 * Is this a directory?
	 */
	[[nodiscard]] bool isDirectory(zstring_view directory);
	[[nodiscard]] bool isDirectory(const Stat& st);

	/**
	 * Does this file (directory) exists?
	 */
	[[nodiscard]] bool exists(zstring_view filename);

	/** Get the date/time of last modification.
	  * Is guaranteed to not be INVALID_TIME_T.
	  */
	[[nodiscard]] inline time_t getModificationDate(const Stat& st) {
		return Date::adjustTimeT(st.st_mtime);
	}

	/**
	 * Gets the next numbered file name with the specified prefix in the
	 * specified directory, with the specified extension. Examples:
	 * automatic numbering of filenames for new screenshots or sound logs.
	 * @param directory Name of the directory in the openMSX user dir in
	 * which should be searched for the next filename
	 * @param prefix Prefix of the filename with numbers
	 * @param extension Extension of the filename with numbers
	 * @param addSeparator Add separator character between 'prefix' and 'number'.
	 */
	[[nodiscard]] std::string getNextNumberedFileName(
		std::string_view directory, std::string_view prefix, std::string_view extension,
		bool addSeparator = false);

	/** Helper function for parsing filename arguments in Tcl commands.
	 * - If argument is empty then getNextNumberedFileName() is used
	 *   with given directory, prefix and extension.
	 * - If argument doesn't already end with the given extension that
	 *   extension is appended.
	 * - If argument doesn't already include a directory, the given
	 *   directory is used (and created if required).
	 */
	[[nodiscard]] std::string parseCommandFileArgument(
		std::string_view argument, std::string_view directory,
		std::string_view prefix,   std::string_view extension);

	/**
	 * Get the name of the temp directory on the system.
	 * Typically /tmp on *nix and C:/WINDOWS/TEMP on windows
	 */
	[[nodiscard]] std::string getTempDir();

	/**
	 * Open a new file with a unique name in the provided directory
	 * @param directory directory in which to open the temp file
	 * @param filename [output param] the name of the resulting file
	 * @result pointer to the opened file
	 */
	[[nodiscard]] FILE_t openUniqueFile(const std::string& directory, std::string& filename);

} // namespace openmsx::FileOperations

#endif
