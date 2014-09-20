#ifndef FILEPOOL_HH
#define FILEPOOL_HH

#include "FileOperations.hh"
#include "Observer.hh"
#include "EventListener.hh"
#include "sha1.hh"
#include "noncopyable.hh"
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <ctime>
#include <cstdint>

namespace openmsx {

class CommandController;
class EventDistributor;
class File;
class Setting;
class StringSetting;
class CliComm;

class FilePool final : private Observer<Setting>, private EventListener
                     , private noncopyable
{
public:
	FilePool(CommandController& controler, EventDistributor& distributor);
	~FilePool();

	enum FileType {
		SYSTEM_ROM = 1, ROM = 2, DISK = 4, TAPE = 8
	};

	/** Search file with the given sha1sum.
	 * If found it returns the (already opened) file,
	 * if not found it returns nullptr.
	 */
	std::unique_ptr<File> getFile(FileType fileType, const Sha1Sum& sha1sum);

	/** Calculate sha1sum for the given File object.
	 * If possible the result is retrieved from cache, avoiding the
	 * relatively expensive calculation.
	 */
	Sha1Sum getSha1Sum(File& file);

	/** Remove sha1sum for this file from the cache.
	 * When the file was written to, sha1sum changes and it should be
	 * removed from the cache.
	 */
	void removeSha1Sum(File& file);

private:
	struct ScanProgress {
		uint64_t lastTime;
		unsigned amountScanned;
	};
	struct Entry {
		std::string path;
		int types;
	};
	typedef std::vector<Entry> Directories;

	// <sha1sum, timestamp, filename>, sorted on sha1sum
	typedef std::vector<std::tuple<Sha1Sum, time_t, std::string>> Pool;

	void insert(const Sha1Sum& sum, time_t time, const std::string& filename);
	void remove(Pool::iterator it);
	bool adjust(Pool::iterator it, const Sha1Sum& newSum);

	void readSha1sums();
	void writeSha1sums();

	std::unique_ptr<File> getFromPool(const Sha1Sum& sha1sum);
	std::unique_ptr<File> scanDirectory(const Sha1Sum& sha1sum,
	                                    const std::string& directory,
	                                    const std::string& poolPath,
	                                    ScanProgress& progress);
	std::unique_ptr<File> scanFile(const Sha1Sum& sha1sum,
	                               const std::string& filename,
	                               const FileOperations::Stat& st,
	                               const std::string& poolPath,
	                               ScanProgress& progress);
	Pool::iterator findInDatabase(const std::string& filename);

	Directories getDirectories() const;

	// Observer<Setting>
	void update(const Setting& setting) override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;


	const std::unique_ptr<StringSetting> filePoolSetting;
	EventDistributor& distributor;
	CliComm& cliComm;

	Pool pool;
	bool quit;
	bool needWrite;
};

} // namespace openmsx

#endif
