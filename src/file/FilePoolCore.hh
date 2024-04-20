#ifndef FILEPOOLCORE_HH
#define FILEPOOLCORE_HH

#include "FileOperations.hh"
#include "ObjectPool.hh"
#include "MemBuffer.hh"
#include "SimpleHashSet.hh"
#include "sha1.hh"
#include "xxhash.hh"
#include <cassert>
#include <cstdint>
#include <ctime>
#include <functional>
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
		Dir() = default;
		Dir(std::string_view p, FileType t)
			: path(p), types(t) {} // clang-15 workaround

		std::string_view path;
		FileType types;
	};
	using Directories = std::vector<Dir>;

public:
	FilePoolCore(std::string fileCache,
	             std::function<Directories()> getDirectories,
	             std::function<void(std::string_view, float)> reportProgress);
	~FilePoolCore();

	/** Search file with the given sha1sum.
	 * If found it returns the (already opened) file,
	 * if not found it returns a close File object.
	 */
	[[nodiscard]] File getFile(FileType fileType, const Sha1Sum& sha1sum);

	/** Calculate sha1sum for the given File object.
	 * If possible the result is retrieved from cache, avoiding the
	 * relatively expensive calculation.
	 */
	[[nodiscard]] Sha1Sum getSha1Sum(File& file);

	/** This is only meaningful to call from within the 'reportProgress'
	 * callback (constructor parameter). This will abort the current search
	 * and cause getFile() to return a not-found result.
	 */
	void abort() { stop = true; }

private:
	struct ScanProgress {
		uint64_t lastTime;
		unsigned amountScanned = 0;
		bool printed = false;
	};

	struct Entry {
		Entry(const Sha1Sum& s, time_t t, std::string_view f)
			: filename(f), time(t), sum(s)
		{
			assert(time != Date::INVALID_TIME_T);
		}
		Entry(const Sha1Sum& s, const char* t, std::string_view f)
			: filename(f), timeStr(t), sum(s)
		{
			assert(timeStr != nullptr);
		}

		[[nodiscard]] time_t getTime();
		void setTime(time_t t);

		// - At least one of 'timeStr' or 'time' is valid.
		// - 'filename' and 'timeStr' are non-owning pointers.
		std::string_view filename;
		const char* timeStr = nullptr; // might be nullptr
		time_t time = Date::INVALID_TIME_T;
		Sha1Sum sum;
	};

	using Pool = ObjectPool<Entry>;
	using Index = Pool::Index;
	using Sha1Index = std::vector<Index>; // sorted on sha1sum

	class FilenameIndexHelper {
	public:
		explicit FilenameIndexHelper(const Pool& p) : pool(p) {}
		[[nodiscard]] std::string_view get(std::string_view s) const { return s; }
		[[nodiscard]] std::string_view get(Index idx) const { return pool[idx].filename; }
	private:
		const Pool& pool;
	};
	struct FilenameIndexHash : FilenameIndexHelper {
		using FilenameIndexHelper::FilenameIndexHelper;
		template<typename T> [[nodiscard]] auto operator()(T t) const {
			XXHasher hasher;
			return hasher(get(t));
		}
	};
	struct FilenameIndexEqual : FilenameIndexHelper {
		using FilenameIndexHelper::FilenameIndexHelper;
		template<typename T1, typename T2>
		[[nodiscard]] bool operator()(T1 x, T2 y) const {
			return get(x) == get(y);
		}
	};
	// Hash indexed by filename, points to a full object in 'pool'
	using FilenameIndex = SimpleHashSet<Index(-1), FilenameIndexHash, FilenameIndexEqual>;

private:
	void insert(const Sha1Sum& sum, time_t time, const std::string& filename);
	[[nodiscard]] Sha1Index::iterator getSha1Iterator(Index idx, const Entry& entry);
	void remove(Sha1Index::iterator it);
	void remove(Index idx);
	void remove(Index idx, const Entry& entry);
	bool adjustSha1(Sha1Index::iterator it, Entry& entry, const Sha1Sum& newSum);
	bool adjustSha1(Index idx,              Entry& entry, const Sha1Sum& newSum);

	void readSha1sums();
	void writeSha1sums();

	[[nodiscard]] File getFromPool(const Sha1Sum& sha1sum);
	[[nodiscard]] File scanDirectory(
		const Sha1Sum& sha1sum,
	        const std::string& directory,
	        std::string_view poolPath,
	        ScanProgress& progress);
	[[nodiscard]] File scanFile(
		const Sha1Sum& sha1sum,
	        const std::string& filename,
	        const FileOperations::Stat& st,
	        std::string_view poolPath,
	        ScanProgress& progress);
	[[nodiscard]] Sha1Sum calcSha1sum(File& file) const;
	[[nodiscard]] std::pair<Index, Entry*> findInDatabase(std::string_view filename);

private:
	std::string fileCache; // path of the '.filecache' file.
	std::function<Directories()> getDirectories;
	std::function<void(std::string_view, float)> reportProgress;

	MemBuffer<char> fileMem; // content of initial .filecache
	std::vector<std::string> stringBuffer; // owns strings that are not in 'fileMem'

	Pool pool; // the actual entries
	Sha1Index sha1Index; // entries accessible via sha1, sorted on 'CompareSha1'
	FilenameIndex filenameIndex{FilenameIndexHash(pool), FilenameIndexEqual(pool)}; // accessible via filename

	bool stop = false; // abort long search (set via reportProgress callback)
	bool needWrite = false; // dirty '.filecache'? write on exit

	friend struct GetSha1;
};

} // namespace openmsx

#endif
