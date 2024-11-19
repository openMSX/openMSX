#include "FilePoolCore.hh"

#include "File.hh"
#include "FileException.hh"
#include "foreach_file.hh"

#include "Date.hh"
#include "Timer.hh"
#include "one_of.hh"
#include "ranges.hh"

#include <fstream>
#include <optional>
#include <tuple>

namespace openmsx {

struct GetSha1 {
	const FilePoolCore::Pool& pool;

	[[nodiscard]] const Sha1Sum& operator()(FilePoolCore::Index idx) const {
		return pool[idx].sum;
	}
};


FilePoolCore::FilePoolCore(std::string fileCache_,
                           std::function<Directories()> getDirectories_,
                           std::function<void(std::string_view, float)> reportProgress_)
	: fileCache(std::move(fileCache_))
	, getDirectories(std::move(getDirectories_))
	, reportProgress(std::move(reportProgress_))
{
	try {
		readSha1sums();
	} catch (MSXException&) {
		// ignore, probably .filecache doesn't exist yet
	}
}

FilePoolCore::~FilePoolCore()
{
	if (needWrite) {
		writeSha1sums();
	}
}

void FilePoolCore::insert(const Sha1Sum& sum, time_t time, const std::string& filename)
{
	stringBuffer.push_back(filename);
	auto idx = pool.emplace(sum, time, stringBuffer.back()).idx;
	auto it = ranges::upper_bound(sha1Index, sum, {}, GetSha1{pool});
	sha1Index.insert(it, idx);
	filenameIndex.insert(idx);
	needWrite = true;
}

FilePoolCore::Sha1Index::iterator FilePoolCore::getSha1Iterator(Index idx, const Entry& entry)
{
	// There can be multiple entries for the same sha1, look for the specific one.
	for (auto [b, e] = ranges::equal_range(sha1Index, entry.sum, {}, GetSha1{pool}); b != e; ++b) {
		if (*b == idx) {
			return b;
		}
	}
	assert(false); return sha1Index.end();
}

void FilePoolCore::remove(Sha1Index::iterator it)
{
	auto idx = *it;
	filenameIndex.erase(idx);
	pool.remove(idx);
	sha1Index.erase(it);
	needWrite = true;
}

void FilePoolCore::remove(Index idx, const Entry& entry)
{
	remove(getSha1Iterator(idx, entry));
}

void FilePoolCore::remove(Index idx)
{
	remove(idx, pool[idx]);
}

// Change the sha1sum of the element pointed to by 'it' into 'newSum'. Also
// re-arrange the items so that 'sha1Index' remains sorted on sha1sum.
// Internally this method doesn't actually sort, it merely rotates the
// elements.
// Returns false if the new position is before (or at) the old position.
// Returns true  if the new position is after          the old position.
bool FilePoolCore::adjustSha1(Sha1Index::iterator it, Entry& entry, const Sha1Sum& newSum)
{
	needWrite = true;
	auto newIt = ranges::upper_bound(sha1Index, newSum, {}, GetSha1{pool});
	entry.sum = newSum; // update sum
	if (newIt > it) {
		// move to back
		rotate(it, it + 1, newIt);
		return true;
	} else {
		if (newIt < it) {
			// move to front
			rotate(newIt, it, it + 1);
		} else {
			// (unlikely) sha1sum has changed, but after
			// resorting item would remain in the same
			// position
		}
		return false;
	}
}

bool FilePoolCore::adjustSha1(Index idx, Entry& entry, const Sha1Sum& newSum)
{
	return adjustSha1(getSha1Iterator(idx, entry), entry, newSum);
}

time_t FilePoolCore::Entry::getTime()
{
	if (time == Date::INVALID_TIME_T) {
		time = Date::fromString(std::span<const char, 24>{timeStr, 24});
	}
	return time;
}

void FilePoolCore::Entry::setTime(time_t t)
{
	time = t;
	timeStr = nullptr;
}

// returns: <sha1, time-string, filename>
static std::optional<std::tuple<Sha1Sum, const char*, std::string_view>> parse(
	std::span<char> line)
{
	if (line.size() <= 68) return {}; // minumum length (only filename is variable)

	// only perform quick sanity check on date/time format
	if (line[40] != ' ') return {}; // two space between sha1sum and date
	if (line[41] != ' ') return {};
	if (line[45] != ' ') return {}; // space between day-of-week and month
	if (line[49] != ' ') return {}; // space between month and day of month
	if (line[52] != ' ') return {}; // space between day of month and hour
	if (line[55] != ':') return {}; // colon between hour and minutes
	if (line[58] != ':') return {}; // colon between minutes and seconds
	if (line[61] != ' ') return {}; // space between seconds and year
	if (line[66] != ' ') return {}; // two spaces between date and filename
	if (line[67] != ' ') return {};

	Sha1Sum sha1(Sha1Sum::UninitializedTag{});
	try {
		sha1.parse40(subspan<40>(line));
	} catch (MSXException&) {
		return {};
	}

	const char* timeStr = &line[42]; // not guaranteed to be a correct date/time
	line[66] = '\0'; // zero-terminate timeStr, so that it can be printed

	std::string_view filename(&line[68], line.size() - 68); // TODO c++20 [begin, end)

	return std::tuple{sha1, timeStr, filename};
}

void FilePoolCore::readSha1sums()
{
	assert(sha1Index.empty());
	assert(fileMem.empty());

	File file(fileCache);
	auto size = file.getSize();
	fileMem.resize(size + 1);
	file.read(fileMem.first(size));
	fileMem[size] = '\n'; // ensure there's always a '\n' at the end

	// Process each line.
	// Assume lines are separated by "\n", "\r\n" or "\n\r" (but not "\r").
	char* data = fileMem.begin();
	char* data_end = fileMem.end();
	while (data != data_end) {
		// memchr() seems better optimized than std::find_if()
		auto* it = static_cast<char*>(memchr(data, '\n', data_end - data));
		if (it == nullptr) it = data_end;
		if ((it != data) && (it[-1] == '\r')) --it;

		if (auto r = parse({data, it})) {
			auto [sum, timeStr, filename] = *r;
			sha1Index.push_back(pool.emplace(sum, timeStr, filename).idx);
			// sha1Index not yet guaranteed sorted
		}

		data = std::find_if(it + 1, data_end, [](char c) {
			return c != one_of('\n', '\r');
		});
	}

	if (!ranges::is_sorted(sha1Index, {}, GetSha1{pool})) {
		// This should _rarely_ happen. In fact it should only happen
		// when .filecache was manually edited. Though because it's
		// very important that pool is indeed sorted I've added this
		// safety mechanism.
		ranges::sort(sha1Index, {}, GetSha1{pool});
	}

	// 'pool' is populated, 'sha1Index' is sorted, now build 'filenameIndex'
	auto n = sha1Index.size();
	filenameIndex.reserve(n);
	while (n != 0) { // sha1Index might change while iterating ...
		--n;
		Index idx = sha1Index[n]; // ... therefor use indices instead of iterators
		bool inserted = filenameIndex.insert(idx);
		if (!inserted) {
			// ERROR: there may not be multiple entries for the same filename
			// Should only happen when .filecache was manually edited.
			remove(idx);
		}
	}
}

void FilePoolCore::writeSha1sums()
{
	std::ofstream file;
	FileOperations::openOfStream(file, fileCache);
	if (!file.is_open()) {
		return;
	}
	for (auto idx : sha1Index) {
		const auto& entry = pool[idx];
		file << entry.sum.toString() << "  ";
		if (entry.timeStr) {
			file << entry.timeStr;
		} else {
			assert(entry.time != Date::INVALID_TIME_T);
			file << Date::toString(entry.time);
		}
		file << "  " << entry.filename << '\n';
	}
}

File FilePoolCore::getFile(FileType fileType, const Sha1Sum& sha1sum)
{
	File result = getFromPool(sha1sum);
	if (result.is_open()) return result;

	// not found in cache, need to scan directories
	stop = false;
	ScanProgress progress {
		.lastTime = Timer::getTime(),
	};

	for (const auto& [path, types] : getDirectories()) {
		if ((types & fileType) != FileType::NONE) {
			result = scanDirectory(sha1sum, FileOperations::expandTilde(std::string(path)), path, progress);
			if (result.is_open()) {
				if (progress.printed) {
					reportProgress(tmpStrCat("Found file with sha1sum ", sha1sum.toString()), 1.0f);
				}
				return result;
			}
		}
	}

	if (progress.printed) {
		reportProgress(tmpStrCat("Did not find file with sha1sum ", sha1sum.toString()), 1.0f);
	}
	return result; // not found
}

Sha1Sum FilePoolCore::calcSha1sum(File& file) const
{
	// Calculate sha1 in several steps so that we can show progress
	// information. We take a fixed step size for an efficient calculation.
	constexpr size_t STEP_SIZE = 1024 * 1024; // 1MB

	auto data = file.mmap();

	SHA1 sha1;
	size_t size = data.size();
	size_t done = 0;
	size_t remaining = size;
	auto lastShowedProgress = Timer::getTime();
	bool everShowedProgress = false;

	auto report = [&](float fraction) {
		reportProgress(tmpStrCat("Calculating SHA1 sum for ", file.getOriginalName()),
		               fraction);
	};
	// Loop over all-but-the last blocks. For small files this loop is skipped.
	while (remaining > STEP_SIZE) {
		sha1.update({&data[done], STEP_SIZE});
		done += STEP_SIZE;
		remaining -= STEP_SIZE;

		auto now = Timer::getTime();
		if ((now - lastShowedProgress) > 250'000) { // 4Hz
			report(float(done) / float(size));
			lastShowedProgress = now;
			everShowedProgress = true;
		}
	}
	// last block
	if (remaining) {
		sha1.update({&data[done], remaining});
	}
	if (everShowedProgress) {
		report(1.0f);
	}
	return sha1.digest();
}

File FilePoolCore::getFromPool(const Sha1Sum& sha1sum)
{
	auto [b, e] = ranges::equal_range(sha1Index, sha1sum, {}, GetSha1{pool});
	// use indices instead of iterators
	auto i    = distance(begin(sha1Index), b);
	auto last = distance(begin(sha1Index), e);
	while (i != last) {
		auto it = begin(sha1Index) + i;
		auto& entry = pool[*it];
		if (entry.getTime() == Date::INVALID_TIME_T) {
			// Invalid time/date format. Remove from
			// database and continue searching.
			remove(it);
			--last;
			continue;
		}
		try {
			File file(std::string(entry.filename));
			auto newTime = file.getModificationDate();
			if (entry.getTime() == newTime) {
				// When modification time is unchanged, assume
				// sha1sum is also unchanged. So avoid
				// expensive sha1sum calculation.
				return file;
			}
			entry.setTime(newTime); // update timestamp
			needWrite = true;
			auto newSum = calcSha1sum(file);
			if (newSum == sha1sum) {
				// Modification time was changed, but
				// (recalculated) sha1sum is still the same.
				return file;
			}
			// Sha1sum has changed: update sha1sum, move entry to
			// new position new sum and continue searching.
			if (adjustSha1(it, entry, newSum)) {
				// after
				--last; // no ++i
			} else {
				// before (or at)
				++i;
			}
		} catch (FileException&) {
			// Error reading file: remove from db and continue
			// searching.
			remove(it);
			--last;
		}
	}
	return {}; // not found
}

File FilePoolCore::scanDirectory(
	const Sha1Sum& sha1sum, const std::string& directory, std::string_view poolPath,
	ScanProgress& progress)
{
	File result;
	auto fileAction = [&](const std::string& path, const FileOperations::Stat& st) {
		if (stop) {
			// Scanning can take a long time. Allow to exit
			// openmsx when it takes too long. Stop scanning
			// by pretending we didn't find the file.
			assert(!result.is_open());
			return false; // abort foreach_file_recursive
		}
		result = scanFile(sha1sum, path, st, poolPath, progress);
		return !result.is_open(); // abort traversal when found
	};
	foreach_file_recursive(directory, fileAction);
	return result;
}

File FilePoolCore::scanFile(const Sha1Sum& sha1sum, const std::string& filename,
                            const FileOperations::Stat& st, std::string_view poolPath,
                            ScanProgress& progress)
{
	++progress.amountScanned;
	// Periodically send a progress message with the current filename
	if (auto now = Timer::getTime();
	    now > (progress.lastTime + 250'000)) { // 4Hz
		progress.lastTime = now;
		progress.printed = true;
		reportProgress(tmpStrCat(
		        "Searching for file with sha1sum ", sha1sum.toString(),
		        "...\nIndexing filepool ", poolPath, ": [",
		        progress.amountScanned, "]: ",
		        std::string_view(filename).substr(poolPath.size())),
		        -1.0f); // unknown progress
	}

	auto time = FileOperations::getModificationDate(st);
	if (auto [idx, entry] = findInDatabase(filename); idx == Index(-1)) {
		// not in pool
		try {
			File file(filename);
			auto sum = calcSha1sum(file);
			insert(sum, time, filename);
			if (sum == sha1sum) {
				return file;
			}
		} catch (FileException&) {
			// ignore
		}
	} else {
		// already in pool
		assert(filename == entry->filename);
		try {
			if (entry->getTime() == time) {
				// db is still up to date
				if (entry->sum == sha1sum) {
					return File(filename);
				}
			} else {
				// db outdated
				File file(filename);
				auto sum = calcSha1sum(file);
				entry->setTime(time);
				adjustSha1(idx, *entry, sum);
				if (sum == sha1sum) {
					return file;
				}
			}
		} catch (FileException&) {
			// error reading file, remove from db
			remove(idx, *entry);
		}
	}
	return {}; // not found
}

std::pair<FilePoolCore::Index, FilePoolCore::Entry*> FilePoolCore::findInDatabase(std::string_view filename)
{
	auto it = filenameIndex.find(filename);
	if (!it) return {Index(-1), nullptr};

	Index idx = *it;
	auto& entry = pool[idx];
	assert(entry.filename == filename);
	if (entry.getTime() == Date::INVALID_TIME_T) {
		// invalid date/time string, remove from db
		remove(idx, entry);
		return {Index(-1), nullptr};
	}
	return {idx, &entry};
}

Sha1Sum FilePoolCore::getSha1Sum(File& file)
{
	auto time = file.getModificationDate();
	const std::string& filename = file.getURL();

	auto [idx, entry] = findInDatabase(filename);
	if ((idx != Index(-1)) && (entry->getTime() == time)) {
		// in database and modification time matches,
		// assume sha1sum also matches
		return entry->sum;
	}

	// not in database or timestamp mismatch
	auto sum = calcSha1sum(file);
	if (idx == Index(-1)) {
		// was not yet in database, insert new entry
		insert(sum, time, filename);
	} else {
		// was already in database, but with wrong timestamp (and sha1sum)
		entry->setTime(time);
		adjustSha1(idx, *entry, sum);
	}
	return sum;
}

} // namespace openmsx
