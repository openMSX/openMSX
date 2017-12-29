#ifndef FILEPOOL_HH
#define FILEPOOL_HH

#include "FileOperations.hh"
#include "StringSetting.hh"
#include "Observer.hh"
#include "EventListener.hh"
#include "MemBuffer.hh"
#include "sha1.hh"
#include <cassert>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace openmsx {

class CommandController;
class Reactor;
class File;
class Sha1SumCommand;

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

	struct PoolEntry {
		PoolEntry(const Sha1Sum& s, time_t t, const char* f)
			: filename(f), time(t), sum(s)
		{
			assert(time != time_t(-1));
		}
		PoolEntry(const Sha1Sum& s, const char* t, const char* f)
			: filename(f), timeStr(t), sum(s)
		{
			assert(timeStr != nullptr);
		}

		time_t getTime();
		void setTime(time_t t);

		// - At least one of 'timeStr' or 'time' is valid.
		// - 'filename' and 'timeStr' are non-owning pointers.
		const char* filename;
		const char* timeStr = nullptr; // might be nullptr
		time_t time = time_t(-1);      // might be -1
		Sha1Sum sum;
	};
	struct ComparePool { // PoolEntry sorted on 'sum'
		bool operator()(const PoolEntry& x, const PoolEntry& y) const {
			return x.sum < y.sum;
		}
		bool operator()(const PoolEntry& x, const Sha1Sum& y) const {
			return x.sum < y;
		}
		bool operator()(const Sha1Sum& x, const PoolEntry& y) const {
			return x < y.sum;
		}
	};
	using Pool = std::vector<PoolEntry>; // sorted with 'ComparePool'

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
	std::unique_ptr<Sha1SumCommand> sha1SumCommand;
	MemBuffer<char> fileMem; // content of initial .filecache
	std::vector<std::string> stringBuffer; // owns strings that are not in 'fileMem'

	Pool pool;
	bool quit;
	bool needWrite;
};

} // namespace openmsx

#endif
