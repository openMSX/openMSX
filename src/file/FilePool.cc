#include "FilePool.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "ReadDir.hh"
#include "Date.hh"
#include "CommandException.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "Reactor.hh"
#include "Timer.hh"
#include "ranges.hh"
#include "sha1.hh"
#include <fstream>
#include <memory>

using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;

namespace openmsx {

class Sha1SumCommand final : public Command
{
public:
	Sha1SumCommand(CommandController& commandController, FilePool& filePool);
	void execute(span<const TclObject> tokens, TclObject& result) override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	FilePool& filePool;
};


const char* const FILE_CACHE = "/.filecache";

static string initialFilePoolSettingValue()
{
	TclObject result;

	for (auto& p : systemFileContext().getPaths()) {
		result.addListElement(
			makeTclDict("-path", FileOperations::join(p, "systemroms"),
			            "-types", "system_rom"),
			makeTclDict("-path", FileOperations::join(p, "software"),
			            "-types", "rom disk tape"));
	}
	return string(result.getString());
}

FilePool::FilePool(CommandController& controller, Reactor& reactor_)
	: filePoolSetting(
		controller, "__filepool",
		"This is an internal setting. Don't change this directly, "
		"instead use the 'filepool' command.",
		initialFilePoolSettingValue())
	, reactor(reactor_)
	, quit(false)
{
	filePoolSetting.attach(*this);
	reactor.getEventDistributor().registerEventListener(OPENMSX_QUIT_EVENT, *this);
	try {
		readSha1sums();
	} catch (MSXException&) {
		// ignore, probably .filecache doesn't exist yet
	}
	needWrite = false;

	sha1SumCommand = std::make_unique<Sha1SumCommand>(controller, *this);
}

FilePool::~FilePool()
{
	if (needWrite) {
		writeSha1sums();
	}
	reactor.getEventDistributor().unregisterEventListener(OPENMSX_QUIT_EVENT, *this);
	filePoolSetting.detach(*this);
}

void FilePool::insert(const Sha1Sum& sum, time_t time, const string& filename)
{
	auto it = ranges::upper_bound(pool, sum, ComparePool());
	stringBuffer.push_back(filename);
	pool.emplace(it, sum, time, stringBuffer.back().c_str());
	needWrite = true;
}

void FilePool::remove(Pool::iterator it)
{
	pool.erase(it);
	needWrite = true;
}

// Change the sha1sum of the element pointed to by 'it' into 'newSum'.
// Also re-arrange the items so that pool remains sorted on sha1sum. Internally
// this method doesn't actually sort, it merely rotates the elements.
// Returns false if the new position is before (or at) the old position.
// Returns true  if the new position is after          the old position.
bool FilePool::adjust(Pool::iterator it, const Sha1Sum& newSum)
{
	needWrite = true;
	auto newIt = ranges::upper_bound(pool, newSum, ComparePool());
	it->sum = newSum; // update sum
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

time_t FilePool::PoolEntry::getTime()
{
	if (time == time_t(-1)) {
		time = Date::fromString(timeStr);
	}
	return time;
}

void FilePool::PoolEntry::setTime(time_t t)
{
	time = t;
	timeStr = nullptr;
}

static bool parse(char* line, char* line_end,
                  Sha1Sum& sha1, const char*& timeStr, const char*& filename)
{
	if ((line_end - line) <= 68) return false; // minumum length (only filename is variable)

	// only perform quick sanity check on date/time format
	if (line[40] != ' ') return false; // two space between sha1sum and date
	if (line[41] != ' ') return false;
	if (line[45] != ' ') return false; // space between day-of-week and month
	if (line[49] != ' ') return false; // space between month and day of month
	if (line[52] != ' ') return false; // space between day of month and hour
	if (line[55] != ':') return false; // colon between hour and minutes
	if (line[58] != ':') return false; // colon between minutes and seconds
	if (line[61] != ' ') return false; // space between seconds and year
	if (line[66] != ' ') return false; // two spaces between date and filename
	if (line[67] != ' ') return false;

	try {
		sha1.parse40(line);
	} catch (MSXException& /*e*/) {
		return false;
	}

	timeStr = line + 42; // not guaranteed to be a correct date/time
	line[66] = '\0'; // zero-terminate timeStr, so that it can be printed

	filename = line + 68;
	*line_end = '\0'; // ok because there is certainly a '\n' after this line
	return true;
}

void FilePool::readSha1sums()
{
	assert(pool.empty());
	assert(fileMem.empty());

	File file(FileOperations::getUserDataDir() + FILE_CACHE);
	auto size = file.getSize();
	fileMem.resize(size + 1);
	file.read(fileMem.data(), size);
	fileMem[size] = '\n'; // ensure there's always a '\n' at the end

	// Process each line.
	// Assume lines are separated by "\n", "\r\n" or "\n\r" (but not "\r").
	char* data = fileMem.data();
	char* data_end = data + size + 1;
	while (data != data_end) {
		// memchr() seems better optimized than std::find_if()
		char* it = static_cast<char*>(memchr(data, '\n', data_end - data));
		if (it == nullptr) it = data_end;
		if ((it != data) && (it[-1] == '\r')) --it;

		Sha1Sum sum(Sha1Sum::UninitializedTag{});
		const char* timeStr;
		const char* filename;
		if (parse(data, it, sum, timeStr, filename)) {
			pool.emplace_back(sum, timeStr, filename);
		}

		data = std::find_if(it + 1, data_end, [](char c) {
			return !(c == '\n' || c == '\r');
		});
	}

	if (!ranges::is_sorted(pool, ComparePool())) {
		// This should _rarely_ happen. In fact it should only happen
		// when .filecache was manually edited. Though because it's
		// very important that pool is indeed sorted I've added this
		// safety mechanism.
		ranges::sort(pool, ComparePool());
	}
}

void FilePool::writeSha1sums()
{
	string cacheFile = FileOperations::getUserDataDir() + FILE_CACHE;
	ofstream file;
	FileOperations::openofstream(file, cacheFile);
	if (!file.is_open()) {
		return;
	}
	for (auto& p : pool) {
		file << p.sum.toString() << "  ";
		if (p.timeStr) {
			file << p.timeStr;
		} else {
			assert(p.time != time_t(-1));
			file << Date::toString(p.time);
		}
		file << "  " << p.filename << '\n';
	}
}

static int parseTypes(Interpreter& interp, const TclObject& list)
{
	int result = 0;
	unsigned num = list.getListLength(interp);
	for (unsigned i = 0; i < num; ++i) {
		std::string_view elem = list.getListIndex(interp, i).getString();
		if (elem == "system_rom") {
			result |= FilePool::SYSTEM_ROM;
		} else if (elem == "rom") {
			result |= FilePool::ROM;
		} else if (elem == "disk") {
			result |= FilePool::DISK;
		} else if (elem == "tape") {
			result |= FilePool::TAPE;
		} else {
			throw CommandException("Unknown type: ", elem);
		}
	}
	return result;
}

void FilePool::update(const Setting& setting)
{
	assert(&setting == &filePoolSetting); (void)setting;
	getDirectories(); // check for syntax errors
}

FilePool::Directories FilePool::getDirectories() const
{
	Directories result;
	auto& interp = filePoolSetting.getInterpreter();
	const TclObject& all = filePoolSetting.getValue();
	unsigned numLines = all.getListLength(interp);
	for (unsigned i = 0; i < numLines; ++i) {
		Entry entry;
		bool hasPath = false;
		entry.types = 0;
		TclObject line = all.getListIndex(interp, i);
		unsigned numItems = line.getListLength(interp);
		if (numItems & 1) {
			throw CommandException(
				"Expected a list with an even number "
				"of elements, but got ", line.getString());
		}
		for (unsigned j = 0; j < numItems; j += 2) {
			std::string_view name  = line.getListIndex(interp, j + 0).getString();
			TclObject value = line.getListIndex(interp, j + 1);
			if (name == "-path") {
				entry.path = value.getString();
				hasPath = true;
			} else if (name == "-types") {
				entry.types = parseTypes(interp, value);
			} else {
				throw CommandException("Unknown item: ", name);
			}
		}
		if (!hasPath) {
			throw CommandException(
				"Missing -path item: ", line.getString());
		}
		if (entry.types == 0) {
			throw CommandException(
				"Missing -types item: ", line.getString());
		}
		result.push_back(entry);
	}
	return result;
}

File FilePool::getFile(FileType fileType, const Sha1Sum& sha1sum)
{
	File result = getFromPool(sha1sum);
	if (result.is_open()) return result;

	// not found in cache, need to scan directories
	ScanProgress progress;
	progress.lastTime = Timer::getTime();
	progress.amountScanned = 0;

	Directories directories;
	try {
		directories = getDirectories();
	} catch (CommandException& e) {
		reactor.getCliComm().printWarning(
			"Error while parsing '__filepool' setting", e.getMessage());
	}
	for (auto& d : directories) {
		if (d.types & fileType) {
			string path = FileOperations::expandTilde(d.path);
			result = scanDirectory(sha1sum, path, d.path, progress);
			if (result.is_open()) return result;
		}
	}

	return result; // not found
}

static void reportProgress(const string& filename, size_t percentage,
                           Reactor& reactor)
{
	reactor.getCliComm().printProgress(
		"Calculating SHA1 sum for ", filename, "... ", percentage, '%');
	reactor.getDisplay().repaint();
}

static Sha1Sum calcSha1sum(File& file, Reactor& reactor)
{
	// Calculate sha1 in several steps so that we can show progress
	// information. We take a fixed step size for an efficient calculation.
	static const size_t STEP_SIZE = 1024 * 1024; // 1MB

	auto data = file.mmap();
	string filename = file.getOriginalName();

	SHA1 sha1;
	size_t size = data.size();
	size_t done = 0;
	size_t remaining = size;
	auto lastShowedProgress = Timer::getTime();
	bool everShowedProgress = false;

	// Loop over all-but-the last blocks. For small files this loop is skipped.
	while (remaining > STEP_SIZE) {
		sha1.update(&data[done], STEP_SIZE);
		done += STEP_SIZE;
		remaining -= STEP_SIZE;

		auto now = Timer::getTime();
		if ((now - lastShowedProgress) > 1000000) {
			reportProgress(filename, (100 * done) / size, reactor);
			lastShowedProgress = now;
			everShowedProgress = true;
		}
	}
	// last block
	sha1.update(&data[done], remaining);
	if (everShowedProgress) {
		reportProgress(filename, 100, reactor);
	}
	return sha1.digest();
}

File FilePool::getFromPool(const Sha1Sum& sha1sum)
{
	auto [b, e] = ranges::equal_range(pool, sha1sum, ComparePool());
	// use indices instead of iterators
	auto i    = distance(begin(pool), b);
	auto last = distance(begin(pool), e);
	while (i != last) {
		auto it = begin(pool) + i;
		if (it->getTime() == time_t(-1)) {
			// Invalid time/date format. Remove from
			// database and continue searching.
			remove(it);
			--last;
			continue;
		}
		try {
			File file(it->filename);
			auto newTime = file.getModificationDate();
			if (it->time == newTime) {
				// When modification time is unchanged, assume
				// sha1sum is also unchanged. So avoid
				// expensive sha1sum calculation.
				return file;
			}
			it->setTime(newTime); // update timestamp
			needWrite = true;
			auto newSum = calcSha1sum(file, reactor);
			if (newSum == sha1sum) {
				// Modification time was changed, but
				// (recalculated) sha1sum is still the same.
				return file;
			}
			// Sha1sum has changed: update sha1sum, move entry to
			// new position new sum and continue searching.
			if (adjust(it, newSum)) {
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
	return File(); // not found
}

File FilePool::scanDirectory(
	const Sha1Sum& sha1sum, const string& directory, const string& poolPath,
	ScanProgress& progress)
{
	ReadDir dir(directory);
	while (dirent* d = dir.getEntry()) {
		if (quit) {
			// Scanning can take a long time. Allow to exit
			// openmsx when it takes too long. Stop scanning
			// by pretending we didn't find the file.
			return File();
		}
		string file = d->d_name;
		string path = strCat(directory, '/', file);
		FileOperations::Stat st;
		if (FileOperations::getStat(path, st)) {
			File result;
			if (FileOperations::isRegularFile(st)) {
				result = scanFile(sha1sum, path, st, poolPath, progress);
			} else if (FileOperations::isDirectory(st)) {
				if ((file != ".") && (file != "..")) {
					result = scanDirectory(sha1sum, path, poolPath, progress);
				}
			}
			if (result.is_open()) return result;
		}
	}
	return File(); // not found
}

File FilePool::scanFile(const Sha1Sum& sha1sum, const string& filename,
                        const FileOperations::Stat& st, const string& poolPath,
                        ScanProgress& progress)
{
	++progress.amountScanned;
	// Periodically send a progress message with the current filename
	auto now = Timer::getTime();
	if (now > (progress.lastTime + 250000)) { // 4Hz
		progress.lastTime = now;
		reactor.getCliComm().printProgress(
                        "Searching for file with sha1sum ",
			sha1sum.toString(), "...\nIndexing filepool ", poolPath,
			": [", progress.amountScanned, "]: ",
			std::string_view(filename).substr(poolPath.size()));
		reactor.getDisplay().repaint();
	}

	// Note: do NOT call 'reactor.getEventDistributor().deliverEvents()'.
	// See comment in ReverseManager::goTo() for more details.

	auto it = findInDatabase(filename);
	if (it == end(pool)) {
		// not in pool
		try {
			File file(filename);
			auto sum = calcSha1sum(file, reactor);
			auto time = FileOperations::getModificationDate(st);
			insert(sum, time, filename);
			if (sum == sha1sum) {
				return file;
			}
		} catch (FileException&) {
			// ignore
		}
	} else {
		// already in pool
		assert(filename == it->filename);
		assert(it->time != time_t(-1));
		try {
			auto time = FileOperations::getModificationDate(st);
			if (it->time == time) {
				// db is still up to date
				if (it->sum == sha1sum) {
					return File(filename);
				}
			} else {
				// db outdated
				File file(filename);
				auto sum = calcSha1sum(file, reactor);
				it->setTime(time);
				adjust(it, sum);
				if (sum == sha1sum) {
					return file;
				}
			}
		} catch (FileException&) {
			// error reading file, remove from db
			remove(it);
		}
	}
	return File(); // not found
}

FilePool::Pool::iterator FilePool::findInDatabase(const string& filename)
{
	// Linear search in pool for filename.
	// Search from back to front because often, soon after this search, we
	// will insert/remove an element from the vector. This requires
	// shifting all elements in the vector starting from a certain
	// position. Starting the search from the back increases the likelihood
	// that the to-be-shifted elements are already in the memory cache.
	auto i = pool.size();
	while (i) {
		--i;
		auto it = begin(pool) + i;
		if (it->filename == filename) {
			// ensure 'time' is valid
			if (it->getTime() == time_t(-1)) {
				// invalid time/date format, remove from db
				// and continue searching
				pool.erase(it);
				continue;
			}
			return it;
		}
	}
	return end(pool); // not found
}

Sha1Sum FilePool::getSha1Sum(File& file)
{
	auto time = file.getModificationDate();
	const auto& filename = file.getURL();

	auto it = findInDatabase(filename);
	assert((it == end(pool)) || (it->time != time_t(-1)));
	if ((it != end(pool)) && (it->time == time)) {
		// in database and modification time matches,
		// assume sha1sum also matches
		return it->sum;
	}

	// not in database or timestamp mismatch
	auto sum = calcSha1sum(file, reactor);
	if (it == end(pool)) {
		// was not yet in database, insert new entry
		insert(sum, time, filename);
	} else {
		// was already in database, but with wrong timestamp (and sha1sum)
		it->setTime(time);
		adjust(it, sum);
	}
	return sum;
}

int FilePool::signalEvent(const std::shared_ptr<const Event>& event)
{
	(void)event; // avoid warning for non-assert compiles
	assert(event->getType() == OPENMSX_QUIT_EVENT);
	quit = true;
	return 0;
}


// class Sha1SumCommand

Sha1SumCommand::Sha1SumCommand(
		CommandController& commandController_, FilePool& filePool_)
	: Command(commandController_, "sha1sum")
	, filePool(filePool_)
{
}

void Sha1SumCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 2, "filename");
	File file(tokens[1].getString());
	result = filePool.getSha1Sum(file).toString();
}

string Sha1SumCommand::help(const vector<string>& /*tokens*/) const
{
	return "Calculate sha1 value for the given file. If the file is "
	       "(g)zipped the sha1 is calculated on the unzipped version.";
}

void Sha1SumCommand::tabCompletion(vector<string>& tokens) const
{
	completeFileName(tokens, userFileContext());
}

} // namespace openmsx
