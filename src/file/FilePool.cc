// $Id$

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
using std::auto_ptr;

namespace openmsx {

const char* const FILE_CACHE = "/.filecache";

static string initialFilePoolSettingValue(CommandController& controller)
{
	TclObject result;

	SystemFileContext context;
	vector<string> paths = context.getPaths(controller);
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		TclObject entry1;
		entry1.addListElement("-path");
		entry1.addListElement(FileOperations::join(*it, "systemroms"));
		entry1.addListElement("-types");
		entry1.addListElement("system_rom");
		result.addListElement(entry1);

		TclObject entry2;
		entry2.addListElement("-path");
		entry2.addListElement(FileOperations::join(*it, "software"));
		entry2.addListElement("-types");
		entry2.addListElement("rom disk tape");
		result.addListElement(entry2);
	}
	return result.getString();
}

FilePool::FilePool(CommandController& controller, EventDistributor& distributor_)
	: filePoolSetting(new StringSetting(controller,
		"__filepool",
		"This is an internal setting. Don't change this directly, "
		"instead use the 'filepool' command.",
		initialFilePoolSettingValue(controller)))
	, distributor(distributor_)
	, cliComm(controller.getCliComm())
	, quit(false)
{
	filePoolSetting->attach(*this);
	distributor.registerEventListener(OPENMSX_QUIT_EVENT, *this);
	readSha1sums();
}

FilePool::~FilePool()
{
	writeSha1sums();
	distributor.unregisterEventListener(OPENMSX_QUIT_EVENT, *this);
	filePoolSetting->detach(*this);
}

void FilePool::insert(const string& sum, time_t time, const string& filename)
{
	Pool::iterator it = pool.insert(make_pair(sum, make_pair(time, filename)));
	reversePool.insert(make_pair(it->second.second, it));
}

void FilePool::remove(Pool::iterator it)
{
	reversePool.erase(it->second.second);
	pool.erase(it);
}

static bool parse(const string& line, string& sha1, time_t& time, string& filename)
{
	if (line.length() <= 68) {
		return false;
	}
	sha1 = line.substr(0, 40);
	time = Date::fromString(line.substr(42, 24));
	filename = line.substr(68);

	return (sha1.find_first_not_of("0123456789abcdef") == string::npos) &&
	       (time != time_t(-1));
}

void FilePool::readSha1sums()
{
	string cacheFile = FileOperations::getUserDataDir() + FILE_CACHE;
	ifstream file(cacheFile.c_str());
	while (file.good()) {
		string line;
		getline(file, line);

		string sum;
		string filename;
		time_t time;
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
	for (Pool::const_iterator it = pool.begin(); it != pool.end(); ++it) {
		file << it->first << "  "                        // sum
		     << Date::toString(it->second.first) << "  " // date
		     << it->second.second                        // filename
		     << endl;
	}
}

static int parseTypes(const TclObject& list)
{
	int result = 0;
	unsigned num = list.getListLength();
	for (unsigned i = 0; i < num; ++i) {
		string elem = list.getListIndex(i).getString();
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
	Directories dummy;
	getDirectories(dummy); // check for syntax errors
}

void FilePool::getDirectories(Directories& result) const
{
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
			string name  = line.getListIndex(j + 0).getString();
			TclObject value = line.getListIndex(j + 1);
			if (name == "-path") {
				entry.path = value.getString();
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
}

auto_ptr<File> FilePool::getFile(FileType fileType, const string& sha1sum)
{
	auto_ptr<File> result;
	result = getFromPool(sha1sum);
	if (result.get()) {
		return result;
	}

	// not found in cache, need to scan directories
	lastTime = Timer::getTime(); // for progress messages
	amountScanned = 0; // also for progress messages
	Directories directories;
	try {
		getDirectories(directories);
	} catch (CommandException& e) {
		cliComm.printWarning("Error while parsing '__filepool' setting" +
			e.getMessage());
	}
	for (Directories::const_iterator it = directories.begin();
	     it != directories.end(); ++it) {
		if (it->types & fileType) {
			string path = FileOperations::expandTilde(it->path);
			result = scanDirectory(sha1sum, path, it->path);
			if (result.get()) {
				return result;
			}
		}
	}

	return result; // not found
}

static string calcSha1sum(File& file)
{
	unsigned size;
	const byte* data = file.mmap(size);
	return SHA1::calc(data, size);
}

auto_ptr<File> FilePool::getFromPool(const string& sha1sum)
{
	pair<Pool::iterator, Pool::iterator> bound = pool.equal_range(sha1sum);
	Pool::iterator it = bound.first;
	while (it != bound.second) {
		time_t& time = it->second.first;
		const string& filename = it->second.second;
		try {
			auto_ptr<File> file(new File(filename));
			time_t newTime = file->getModificationDate();
			if (time == newTime) {
				// When modification time is unchanged, assume
				// sha1sum is also unchanged. So avoid
				// expensive sha1sum calculation.
				return file;
			}
			string newSum = calcSha1sum(*file);
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
	return auto_ptr<File>(); // not found
}

auto_ptr<File> FilePool::scanDirectory(const string& sha1sum, const string& directory, const string& poolPath)
{
	ReadDir dir(directory);
	while (dirent* d = dir.getEntry()) {
		if (quit) {
			// Scanning can take a long time. Allow to exit
			// openmsx when it takes too long. Stop scanning
			// by pretending we didn't find the file.
			return auto_ptr<File>();
		}
		string file = d->d_name;
		string path = directory + '/' + file;
		FileOperations::Stat st;
		if (FileOperations::getStat(path, st)) {
			auto_ptr<File> result;
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
	return auto_ptr<File>(); // not found
}

auto_ptr<File> FilePool::scanFile(const string& sha1sum, const string& filename,
                                  const FileOperations::Stat& st, const string& poolPath)
{
	amountScanned++;
	// Periodically send a progress message with the current filename
	unsigned long long now = Timer::getTime();
	if (now > (lastTime + 250000)) { // 4Hz
		lastTime = now;
		cliComm.printProgress("Creating filepool index for " +
			poolPath + "...\nScanned " +
			StringOp::toString(amountScanned) +
			" files, current: " + filename);
	}

	// deliverEvents() is relatively cheap when there are no events to
	// deliver, so it's ok to call on each file.
	distributor.deliverEvents();

	Pool::iterator it = findInDatabase(filename);
	if (it == pool.end()) {
		// not in pool
		try {
			auto_ptr<File> file(new File(filename));
			string sum = calcSha1sum(*file);
			time_t time = FileOperations::getModificationDate(st);
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
			time_t time = FileOperations::getModificationDate(st);
			if (time == it->second.first) {
				// db is still up to date
				if (it->first == sha1sum) {
					auto_ptr<File> file(new File(filename));
					return file;
				}
			} else {
				// db outdated
				auto_ptr<File> file(new File(filename));
				string sum = calcSha1sum(*file);
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
	return auto_ptr<File>(); // not found
}

FilePool::Pool::iterator FilePool::findInDatabase(const string& filename)
{
	ReversePool::iterator it = reversePool.find(filename);
	if (it != reversePool.end()) {
		return it->second;
	}
	return pool.end();
}


string FilePool::getSha1Sum(File& file)
{
	time_t time = file.getModificationDate();
	string filename = file.getURL();

	Pool::iterator it = findInDatabase(filename);
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
	unsigned size;
	const byte* data = file.mmap(size);
	string sum = SHA1::calc(data, size);
	insert(sum, time, filename);
	return sum;
}

void FilePool::removeSha1Sum(File& file)
{
	Pool::iterator it = findInDatabase(file.getURL());
	if (it != pool.end()) {
		remove(it);
	}
}

int FilePool::signalEvent(shared_ptr<const Event> event)
{
	(void)event; // avoid warning for non-assert compiles
	assert(event->getType() == OPENMSX_QUIT_EVENT);
	quit = true;
	return 0;
}

} // namespace openmsx
