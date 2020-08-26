#ifndef FILEPOOLCORE_HH
#define FILEPOOLCORE_HH

#include "FileOperations.hh"
#include "MemBuffer.hh"
#include "sha1.hh"
#include <cassert>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class File;

enum class FileType {
	NONE = 0,
	SYSTEM_ROM = 1, ROM = 2, DISK = 4, TAPE = 8
};
inline FileType operator|(FileType x, FileType y) {
	return static_cast<FileType>(int(x) | int(y));
}
inline FileType operator&(FileType x, FileType y) {
	return static_cast<FileType>(int(x) & int(y));
}
inline FileType& operator|=(FileType& x, FileType y) {
	x = x | y;
	return x;
}

class FilePoolCore
{
public:
	struct Dir {
		std::string path;
		FileType types;
	};
	using Directories = std::vector<Dir>;

public:
	FilePoolCore(std::string filecache,
	             std::function<Directories()> getDirectories,
		     std::function<void(const std::string&)> reportProgress);
	~FilePoolCore();

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

	/**/
	void abort() { stop = true; }

private:
	struct ScanProgress {
		uint64_t lastTime;
		unsigned amountScanned;
	};

	struct PoolEntry {
		PoolEntry(const Sha1Sum& s, time_t t, std::string_view f)
			: filename(f), time(t), sum(s)
		{
			assert(time != time_t(-1));
		}
		PoolEntry(const Sha1Sum& s, const char* t, std::string_view f)
			: filename(f), timeStr(t), sum(s)
		{
			assert(timeStr != nullptr);
		}

		time_t getTime();
		void setTime(time_t t);

		// - At least one of 'timeStr' or 'time' is valid.
		// - 'filename' and 'timeStr' are non-owning pointers.
		std::string_view filename;
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

	Sha1Sum calcSha1sum(File& file);
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

private:
	std::string filecache;
	std::function<Directories()> getDirectories;
	std::function<void(std::string)> reportProgress;

	MemBuffer<char> fileMem; // content of initial .filecache
	std::vector<std::string> stringBuffer; // owns strings that are not in 'fileMem'

	Pool pool;

	bool stop = false;
	bool needWrite = false;
};

} // namespace openmsx

#endif
