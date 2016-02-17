#include "FilePool.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "ReadDir.hh"
#include "Date.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "Reactor.hh"
#include "Timer.hh"
#include "StringOp.hh"
#include "memory.hh"
#include "sha1.hh"
#include "stl.hh"
#include <fstream>
#include <cassert>

using std::ifstream;
using std::get;
using std::make_tuple;
using std::ofstream;
using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;

namespace openmsx {

const char* const FILE_CACHE = "/.filecache";

static string initialFilePoolSettingValue()
{
	TclObject result;

	for (auto& p : systemFileContext().getPaths()) {
		TclObject entry1;
		entry1.addListElement("-path");
		entry1.addListElement(FileOperations::join(p, "systemroms"));
		entry1.addListElement("-types");
		entry1.addListElement("system_rom");
		result.addListElement(entry1);

		TclObject entry2;
		entry2.addListElement("-path");
		entry2.addListElement(FileOperations::join(p, "software"));
		entry2.addListElement("-types");
		entry2.addListElement("rom disk tape");
		result.addListElement(entry2);
	}
	return result.getString().str();
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
	readSha1sums();
	needWrite = false;
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
	auto it = upper_bound(begin(pool), end(pool), sum,
	                      LessTupleElement<0>());
	pool.insert(it, make_tuple(sum, time, filename));
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
	auto newIt = upper_bound(begin(pool), end(pool), newSum,
	                         LessTupleElement<0>());
	get<0>(*it) = newSum; // update sum
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

static bool parse(const string& line, Sha1Sum& sha1, time_t& time, string& filename)
{
	if (line.size() <= 68) return false;

	try {
		sha1.parse40(line.data());
	} catch (MSXException& /*e*/) {
		return false;
	}

	time = Date::fromString(line.data() + 42);
	if (time == time_t(-1)) return false;

	filename.assign(line, 68, line.size());
	return true;
}

void FilePool::readSha1sums()
{
	assert(pool.empty());

	string cacheFile = FileOperations::getUserDataDir() + FILE_CACHE;
	ifstream file(cacheFile.c_str());
	string line;
	Sha1Sum sum;
	string filename;
	time_t time;
	while (file.good()) {
		getline(file, line);
		if (parse(line, sum, time, filename)) {
			pool.emplace_back(sum, time, filename);
		}
	}

	if (!std::is_sorted(begin(pool), end(pool), LessTupleElement<0>())) {
		// This should _rarely_ happen. In fact it should only happen
		// when .filecache was manually edited. Though because it's
		// very important that pool is indeed sorted I've added this
		// safety mechanism.
		sort(begin(pool), end(pool), LessTupleElement<0>());
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
		file << get<0>(p).toString()      << "  " // sum
		     << Date::toString(get<1>(p)) << "  " // date
		     << get<2>(p)                         // filename
		     << '\n';
	}
}

static int parseTypes(Interpreter& interp, const TclObject& list)
{
	int result = 0;
	unsigned num = list.getListLength(interp);
	for (unsigned i = 0; i < num; ++i) {
		string_ref elem = list.getListIndex(interp, i).getString();
		if (elem == "system_rom") {
			result |= FilePool::SYSTEM_ROM;
		} else if (elem == "rom") {
			result |= FilePool::ROM;
		} else if (elem == "disk") {
			result |= FilePool::DISK;
		} else if (elem == "tape") {
			result |= FilePool::TAPE;
		} else {
			throw CommandException("Unknown type: " + elem);
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
				"of elements, but got " + line.getString());
		}
		for (unsigned j = 0; j < numItems; j += 2) {
			string_ref name  = line.getListIndex(interp, j + 0).getString();
			TclObject value = line.getListIndex(interp, j + 1);
			if (name == "-path") {
				entry.path = value.getString().str();
				hasPath = true;
			} else if (name == "-types") {
				entry.types = parseTypes(interp, value);
			} else {
				throw CommandException(
					"Unknown item: " + name);
			}
		}
		if (!hasPath) {
			throw CommandException(
				"Missing -path item: " + line.getString());
		}
		if (entry.types == 0) {
			throw CommandException(
				"Missing -types item: " + line.getString());
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
			"Error while parsing '__filepool' setting" + e.getMessage());
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
		"Calculating SHA1 sum for " + filename + "... " + StringOp::toString(percentage) + '%');
	reactor.getDisplay().repaint();
}

static Sha1Sum calcSha1sum(File& file, Reactor& reactor)
{
	// Calculate sha1 in several steps so that we can show progress
	// information. We take a fixed step size for an efficient calculation.
	static const size_t STEP_SIZE = 1024 * 1024; // 1MB

	size_t size;
	const byte* data = file.mmap(size);
	string filename = file.getOriginalName();

	SHA1 sha1;
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
	auto bound = equal_range(begin(pool), end(pool), sha1sum,
	                         LessTupleElement<0>());
	// use indices instead of iterators
	auto i    = distance(begin(pool), bound.first);
	auto last = distance(begin(pool), bound.second);
	while (i != last) {
		auto it = begin(pool) + i;
		auto& time           = get<1>(*it);
		const auto& filename = get<2>(*it);
		try {
			File file(filename);
			auto newTime = file.getModificationDate();
			if (time == newTime) {
				// When modification time is unchanged, assume
				// sha1sum is also unchanged. So avoid
				// expensive sha1sum calculation.
				return file;
			}
			time = newTime; // update timestamp
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
		string path = directory + '/' + file;
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
		reactor.getCliComm().printProgress("Searching for file with sha1sum " +
			sha1sum.toString() + "...\nIndexing filepool " + poolPath +
			": [" + StringOp::toString(progress.amountScanned) + "]: " +
			filename.substr(poolPath.size()));
	}

	// deliverEvents() is relatively cheap when there are no events to
	// deliver, so it's ok to call on each file.
	reactor.getEventDistributor().deliverEvents();

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
		assert(filename == get<2>(*it));
		try {
			auto time = FileOperations::getModificationDate(st);
			if (time == get<1>(*it)) {
				// db is still up to date
				if (get<0>(*it) == sha1sum) {
					return File(filename);
				}
			} else {
				// db outdated
				File file(filename);
				auto sum = calcSha1sum(file, reactor);
				get<1>(*it) = time;
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
	for (auto it = pool.rbegin(); it != pool.rend(); ++it) {
		if (get<2>(*it) == filename) {
			return it.base() - 1;
		}
	}
	return end(pool); // not found
}

Sha1Sum FilePool::getSha1Sum(File& file)
{
	auto time = file.getModificationDate();
	const auto& filename = file.getURL();

	auto it = findInDatabase(filename);
	if ((it != end(pool)) && (get<1>(*it) == time)) {
		// in database and modification time matches,
		// assume sha1sum also matches
		return get<0>(*it);
	}

	// not in database or timestamp mismatch
	auto sum = calcSha1sum(file, reactor);
	if (it == end(pool)) {
		// was not yet in database, insert new entry
		insert(sum, time, filename);
	} else {
		// was already in database, but with wrong timestamp (and sha1sum)
		get<1>(*it) = time;
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

} // namespace openmsx
