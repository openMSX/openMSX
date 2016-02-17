#ifndef FILEPOOL_HH
#define FILEPOOL_HH

#include "FileOperations.hh"
#include "StringSetting.hh"
#include "Observer.hh"
#include "EventListener.hh"
#include "sha1.hh"
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <ctime>
#include <cstdint>

namespace openmsx {

class CommandController;
class Reactor;
class File;

class FilePool final : private Observer<Setting>, private EventListener
{
public:
	FilePool(CommandController& controler, Reactor& reactor);
	~FilePool();

	enum FileType {
		SYSTEM_ROM = 1, ROM = 2, DISK = 4, TAPE = 8
	};

	/** Search file with the given sha1sum.
	 * If found it returns the (already opened) file,
	 * if not found it returns nullptr.
	 */
	File getFile(FileType fileType, const Sha1Sum& sha1sum);

	/** Calculate sha1sum for the given File object.
	 * If possible the result is retrieved from cache, avoiding the
	 * relatively expensive calculation.
	 */
	Sha1Sum getSha1Sum(File& file);

private:
	struct ScanProgress {
		uint64_t lastTime;
		unsigned amountScanned;
	};
	struct Entry {
		std::string path;
		int types;
	};
	using Directories = std::vector<Entry>;

	// <sha1sum, timestamp, filename>, sorted on sha1sum
	using Pool = std::vector<std::tuple<Sha1Sum, time_t, std::string>>;

	void insert(const Sha1Sum& sum, time_t time, const std::string& filename);
	void remove(Pool::iterator it);
	bool adjust(Pool::iterator it, const Sha1Sum& newSum);

	void readSha1sums();
	void writeSha1sums();

	File getFromPool(const Sha1Sum& sha1sum);
	File scanDirectory(const Sha1Sum& sha1sum,
	                   const std::string& directory,
	                   const std::string& poolPath,
	                   ScanProgress& progress);
	File scanFile(const Sha1Sum& sha1sum,
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


	StringSetting filePoolSetting;
	Reactor& reactor;

	Pool pool;
	bool quit;
	bool needWrite;
};

} // namespace openmsx

#endif
