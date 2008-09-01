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
#include <fstream>
#include <cassert>

using std::endl;
using std::ifstream;
using std::make_pair;
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
	auto it = pool.insert(make_pair(sum, make_pair(time, filename)));
	reversePool.insert(make_pair(it->second.second, it));
	needWrite = true;
}

void FilePool::remove(Pool::iterator it)
{
	reversePool.erase(it->second.second);
	pool.erase(it);
	needWrite = true;
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
	string cacheFile = FileOperations::getUserDataDir() + FILE_CACHE;
	ifstream file(cacheFile.c_str());
	string line;
	Sha1Sum sum;
	string filename;
	time_t time;
	while (file.good()) {
		getline(file, line);
		if (parse(line, sum, time, filename)) {
			insert(sum, time, filename);
		}
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
		file << p.first.toString() << "  "             // sum
		     << Date::toString(p.second.first) << "  " // date
		     << p.second.second                        // filename
		     << '\n';
	}
}

static int parseTypes(const TclObject& list)
{
	int result = 0;
	unsigned num = list.getListLength();
	for (unsigned i = 0; i < num; ++i) {
		string_ref elem = list.getListIndex(i).getString();
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
	TclObject all(filePoolSetting->getValue());
	unsigned numLines = all.getListLength();
	for (unsigned i = 0; i < numLines; ++i) {
		Entry entry;
		bool hasPath = false;
		entry.types = 0;
		TclObject line = all.getListIndex(i);
		unsigned numItems = line.getListLength();
		if (numItems & 1) {
			throw CommandException(
				"Expected a list with an even number "
				"of elements, but got " + line.getString());
		}
		for (unsigned j = 0; j < numItems; j += 2) {
			string_ref name  = line.getListIndex(j + 0).getString();
			TclObject value = line.getListIndex(j + 1);
			if (name == "-path") {
				entry.path = value.getString().str();
				hasPath = true;
			} else if (name == "-types") {
				entry.types = parseTypes(value);
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
	if (result.get()) {
		return result;
	}

	// not found in cache, need to scan directories
	lastTime = Timer::getTime(); // for progress messages
	amountScanned = 0; // also for progress messages
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
			result = scanDirectory(sha1sum, path, d.path);
			if (result.get()) {
				return result;
			}
		}
	}

	return result; // not found
}

static Sha1Sum calcSha1sum(File& file, CliComm& cliComm, EventDistributor& distributor)
{
	size_t size;
	const byte* data = file.mmap(size);
	return SHA1::calcWithProgress(data, size, file.getOriginalName(), cliComm, distributor);
}

unique_ptr<File> FilePool::getFromPool(const Sha1Sum& sha1sum)
{
	auto bound = pool.equal_range(sha1sum);
	auto it = bound.first;
	while (it != bound.second) {
		auto& time = it->second.first;
		const auto& filename = it->second.second;
		try {
			auto file = make_unique<File>(filename);
			auto newTime = file->getModificationDate();
			if (time == newTime) {
				// When modification time is unchanged, assume
				// sha1sum is also unchanged. So avoid
				// expensive sha1sum calculation.
				return file;
			}
			auto newSum = calcSha1sum(*file, cliComm, distributor);
			if (newSum == sha1sum) {
				// Modification time was changed, but
				// (recalculated) sha1sum is still the same,
				// only update timestamp.
				time = newTime;
				return file;
			}
			// Did not match: update db with new sum and continue
			// searching.
			remove(it++);
			insert(newSum, newTime, filename);
		} catch (FileException&) {
			// Error reading file: remove from db and continue
			// searching.
			remove(it++);
		}
	}
	return nullptr; // not found
}

unique_ptr<File> FilePool::scanDirectory(const Sha1Sum& sha1sum, const string& directory, const string& poolPath)
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
				result = scanFile(sha1sum, path, st, poolPath);
			} else if (FileOperations::isDirectory(st)) {
				if ((file != ".") && (file != "..")) {
					result = scanDirectory(sha1sum, path, poolPath);
				}
			}
			if (result.get()) {
				return result;
			}
		}
	}
	return nullptr; // not found
}

unique_ptr<File> FilePool::scanFile(const Sha1Sum& sha1sum, const string& filename,
                                  const FileOperations::Stat& st, const string& poolPath)
{
	amountScanned++;
	// Periodically send a progress message with the current filename
	auto now = Timer::getTime();
	if (now > (lastTime + 250000)) { // 4Hz
		lastTime = now;
		cliComm.printProgress("Searching for file with sha1sum " +
			sha1sum.toString() + "...\nIndexing filepool " + poolPath +
			": [" + StringOp::toString(amountScanned) + "]: " +
			filename.substr(poolPath.size()));
	}

	// deliverEvents() is relatively cheap when there are no events to
	// deliver, so it's ok to call on each file.
	distributor.deliverEvents();

	auto it = findInDatabase(filename);
	if (it == pool.end()) {
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
		assert(filename == it->second.second);
		try {
			auto time = FileOperations::getModificationDate(st);
			if (time == it->second.first) {
				// db is still up to date
				if (it->first == sha1sum) {
					return make_unique<File>(filename);
				}
			} else {
				// db outdated
				auto file = make_unique<File>(filename);
				auto sum = calcSha1sum(*file, cliComm, distributor);
				remove(it);
				insert(sum, time, filename);
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
	auto it = reversePool.find(filename);
	if (it != reversePool.end()) {
		return it->second;
	}
	return pool.end();
}


Sha1Sum FilePool::getSha1Sum(File& file)
{
	auto time = file.getModificationDate();
	const auto& filename = file.getURL();

	auto it = findInDatabase(filename);
	if (it != pool.end()) {
		// in database
		if (time == it->second.first) {
			// modification time matches, assume sha1sum also matches
			return it->first;
		} else {
			// mismatch, remove from db and re-calculate
			remove(it);
		}
	}
	// not in db (or timestamp mismatch)
	auto sum = calcSha1sum(file, cliComm, distributor);
	insert(sum, time, filename);
	return sum;
}

void FilePool::removeSha1Sum(File& file)
{
	auto it = findInDatabase(file.getURL());
	if (it != pool.end()) {
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
