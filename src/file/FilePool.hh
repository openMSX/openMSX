// $Id$

#ifndef FILEPOOL_HH
#define FILEPOOL_HH

#include "FileOperations.hh"
#include "noncopyable.hh"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <ctime>

namespace openmsx {

class SettingsConfig;
class File;

class FilePool : private noncopyable
{
public:
	explicit FilePool(SettingsConfig& settingsConfig);
	~FilePool();

	/** Search file with the given sha1sum.
	 * If found it returns the (already opened) file,
	 * if not found it returns a NULL pointer.
	 */
	std::auto_ptr<File> getFile(const std::string& sha1sum);

	/** Calculate sha1sum for the given File object.
	 * If possible the result is retrieved from cache, avoiding the
	 * relatively expensive calculation.
	 */
	std::string getSha1Sum(File& file);

	/** Remove sha1sum for this file from the cache.
	 * When the file was written to, sha1sum changes and it should be
	 * removed from the cache.
	 */
	void removeSha1Sum(File& file);

private:
	typedef std::multimap<std::string, std::pair<time_t, std::string> > Pool;
	typedef std::vector<std::string> Directories;

	void readSha1sums();
	void writeSha1sums();

	std::auto_ptr<File> getFromPool(const std::string& sha1sum);
	std::auto_ptr<File> scanDirectory(const std::string& sha1sum,
	                                  const std::string& directory);
	std::auto_ptr<File> scanFile(const std::string& sha1sum,
	                             const std::string& filename,
	                             const FileOperations::Stat& st);
	Pool::iterator findInDatabase(const std::string& filename);

	Pool pool;
	Directories directories;
};

} // namespace openmsx

#endif
