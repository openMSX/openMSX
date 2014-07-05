#include "FilePool.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "StringSetting.hh"
#include "ReadDir.hh"
#include "Date.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
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
	for (auto& p : SystemFileContext().getPaths()) {
		result.addListElement(TclObject({
			string_ref("-path"),
			string_ref(FileOperations::join(p, "systemroms")),
			string_ref("-types"),
			string_ref("system_rom")}));
		result.addListElement(TclObject({
			string_ref("-path"),
			string_ref(FileOperations::join(p, "software")),
			string_ref("-types"),
			string_ref("rom disk tape")}));
	}
	return result.getString().str();
}

FilePool::FilePool(CommandController& controller, EventDistributor& distributor_)
	: filePoolSetting(make_unique<StringSetting>(
		controller, "__filepool",
		"This is an internal setting. Don't change this directly, "
		"instead use the 'filepool' command.",
		initialFilePoolSettingValue()))
	, distributor(distributor_)
	, cliComm(controller.getCliComm())
	, quit(false)
{
	filePoolSetting->attach(*this);
	distributor.registerEventListener(OPENMSX_QUIT_EVENT, *this);
	readSha1sums();
	needWrite = false;
}

FilePool::~FilePool()
{
	if (needWrite) {
		writeSha1sums();
	}
	distributor.unregisterEventListener(OPENMSX_QUIT_EVENT, *this);
	filePoolSetting->detach(*this);
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
	assert(&setting == filePoolSetting.get()); (void)setting;
	getDirectories(); // check for syntax errors
}

FilePool::Directories FilePool::getDirectories() const
{
	Directories result;
	auto& interp = filePoolSetting->getInterpreter();
	const TclObject& all = filePoolSetting->getValue();
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

unique_ptr<File> FilePool::getFile(FileType fileType, const Sha1Sum& sha1sum)
{
	unique_ptr<File> result;
	result = getFromPool(sha1sum);
	if (result) return result;

	// not found in cache, need to scan directories
	ScanProgress progress;
	progress.lastTime = Timer::getTime();
	progress.amountScanned = 0;

	Directories directories;
	try {
		directories = getDirectories();
	} catch (CommandException& e) {
		cliComm.printWarning("Error while parsing '__filepool' setting" +
			e.getMessage());
	}
	for (auto& d : directories) {
		if (d.types & fileType) {
			string path = FileOperations::expandTilde(d.path);
			result = scanDirectory(sha1sum, path, d.path, progress);
			if (result) return result;
		}
	}

	return result; // not found
}

static void reportProgress(const string& filename, size_t percentage,
                           CliComm& cliComm, EventDistributor& distributor)
{
	cliComm.printProgress(
		"Calculating SHA1 sum for " + filename + "... " + StringOp::toString(percentage) + "%");
	distributor.deliverEvents();
}

static Sha1Sum calcSha1sum(File& file, CliComm& cliComm, EventDistributor& distributor)
{
	size_t size;
	const byte* data = file.mmap(size);

	if (size < 10*1024*1024) {
		// for small files, don't show progress
		return SHA1::calc(data, size);
	}

	// Calculate sha1 in several steps so that we can show progress information
	SHA1 sha1;
	static const size_t NUMBER_OF_STEPS = 100;
	// calculate in NUMBER_OF_STEPS steps and report progress every step
	auto stepSize  = size / NUMBER_OF_STEPS;
	auto remainder = size % NUMBER_OF_STEPS;
	size_t offset = 0;
	string filename = file.getOriginalName();
	reportProgress(filename, 0, cliComm, distributor);
	for (size_t i = 0; i < (NUMBER_OF_STEPS - 1); ++i) {
		sha1.update(&data[offset], stepSize);
		offset += stepSize;
		reportProgress(filename, i + 1, cliComm, distributor);
	}
	sha1.update(data + offset, stepSize + remainder);
	reportProgress(filename, 100, cliComm, distributor);
	return sha1.digest();
}

unique_ptr<File> FilePool::getFromPool(const Sha1Sum& sha1sum)
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
			auto file = make_unique<File>(filename);
			auto newTime = file->getModificationDate();
			if (time == newTime) {
				// When modification time is unchanged, assume
				// sha1sum is also unchanged. So avoid
				// expensive sha1sum calculation.
				return file;
			}
			time = newTime; // update timestamp
			needWrite = true;
			auto newSum = calcSha1sum(*file, cliComm, distributor);
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
	return nullptr; // not found
}

unique_ptr<File> FilePool::scanDirectory(
	const Sha1Sum& sha1sum, const string& directory, const string& poolPath,
	ScanProgress& progress)
{
	ReadDir dir(directory);
	while (dirent* d = dir.getEntry()) {
		if (quit) {
			// Scanning can take a long time. Allow to exit
			// openmsx when it takes too long. Stop scanning
			// by pretending we didn't find the file.
			return nullptr;
		}
		string file = d->d_name;
		string path = directory + '/' + file;
		FileOperations::Stat st;
		if (FileOperations::getStat(path, st)) {
			unique_ptr<File> result;
			if (FileOperations::isRegularFile(st)) {
				result = scanFile(sha1sum, path, st, poolPath, progress);
			} else if (FileOperations::isDirectory(st)) {
				if ((file != ".") && (file != "..")) {
					result = scanDirectory(sha1sum, path, poolPath, progress);
				}
			}
			if (result) return result;
		}
	}
	return nullptr; // not found
}

unique_ptr<File> FilePool::scanFile(const Sha1Sum& sha1sum, const string& filename,
                                    const FileOperations::Stat& st, const string& poolPath,
                                    ScanProgress& progress)
{
	++progress.amountScanned;
	// Periodically send a progress message with the current filename
	auto now = Timer::getTime();
	if (now > (progress.lastTime + 250000)) { // 4Hz
		progress.lastTime = now;
		cliComm.printProgress("Searching for file with sha1sum " +
			sha1sum.toString() + "...\nIndexing filepool " + poolPath +
			": [" + StringOp::toString(progress.amountScanned) + "]: " +
			filename.substr(poolPath.size()));
	}

	// deliverEvents() is relatively cheap when there are no events to
	// deliver, so it's ok to call on each file.
	distributor.deliverEvents();

	auto it = findInDatabase(filename);
	if (it == end(pool)) {
		// not in pool
		try {
			auto file = make_unique<File>(filename);
			auto sum = calcSha1sum(*file, cliComm, distributor);
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
					return make_unique<File>(filename);
				}
			} else {
				// db outdated
				auto file = make_unique<File>(filename);
				auto sum = calcSha1sum(*file, cliComm, distributor);
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
	return nullptr; // not found
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
	auto sum = calcSha1sum(file, cliComm, distributor);
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

void FilePool::removeSha1Sum(File& file)
{
	auto it = findInDatabase(file.getURL());
	if (it != end(pool)) {
		remove(it);
	}
}

int FilePool::signalEvent(const std::shared_ptr<const Event>& event)
{
	(void)event; // avoid warning for non-assert compiles
	assert(event->getType() == OPENMSX_QUIT_EVENT);
	quit = true;
	return 0;
}

} // namespace openmsx
