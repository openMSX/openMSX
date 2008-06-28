// $Id$

#include "FilePool.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include "Date.hh"
#include "SettingsConfig.hh"
#include "XMLElement.hh"
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

FilePool::FilePool(SettingsConfig& settingsConfig)
{
	if (const XMLElement* config =
			settingsConfig.getXMLElement().findChild("RomPool")) {
		XMLElement::Children dirs;
		config->getChildren("directory", dirs);
		for (XMLElement::Children::const_iterator it = dirs.begin();
		     it != dirs.end(); ++it) {
			string dir = FileOperations::expandTilde((*it)->getData());
			directories.push_back(dir);
		}
	}
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	vector<string> paths = context.getPaths(*controller);
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		string dir = FileOperations::join(*it, "systemroms");
		directories.push_back(dir);
	}

	readSha1sums();
}

FilePool::~FilePool()
{
	writeSha1sums();
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

static auto_ptr<File> calcSha1sum(const string& filename, string& sum)
{
	auto_ptr<File> file(new File(filename));
	SHA1 sha1;
	sha1.update(file->mmap(), file->getSize());
	sum = sha1.hex_digest();
	return file;
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
			pool.insert(make_pair(sum, make_pair(time, filename)));
		}
	}
}

void FilePool::writeSha1sums()
{
	string cacheFile = FileOperations::getUserDataDir() + FILE_CACHE;
	ofstream file(cacheFile.c_str());
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

auto_ptr<File> FilePool::getFile(const string& sha1sum)
{
	auto_ptr<File> result;
	result = getFromPool(sha1sum);
	if (result.get()) {
		return result;
	}

	for (Directories::const_iterator it = directories.begin();
	     it != directories.end(); ++it) {
		result = scanDirectory(sha1sum, *it);
		if (result.get()) {
			return result;
		}
	}

	return result; // not found
}

auto_ptr<File> FilePool::getFromPool(const string& sha1sum)
{
	pair<Pool::iterator, Pool::iterator> bound = pool.equal_range(sha1sum);
	Pool::iterator it = bound.first;
	while (it != bound.second) {
		time_t& time = it->second.first;
		const string& filename = it->second.second;
		try {
			string newSum;
			auto_ptr<File> file = calcSha1sum(filename, newSum);
			time_t newTime = file->getModificationDate();
			if (newSum == sha1sum) {
				time = newTime;
				return file;
			}
			// did not match, update db with new sum
			pool.erase(it++);
			pool.insert(make_pair(newSum,
			                 make_pair(newTime, filename)));
		} catch (FileException& e) {
			// error reading file, remove from db
			pool.erase(it++);
		}
	}
	return auto_ptr<File>(); // not found
}

auto_ptr<File> FilePool::scanDirectory(const string& sha1sum, const string& directory)
{
	ReadDir dir(directory);
	while (dirent* d = dir.getEntry()) {
		string file = d->d_name;
		string path = directory + '/' + file;
		auto_ptr<File> result;
		if (FileOperations::isRegularFile(path)) {
			result = scanFile(sha1sum, path);
		} else if (FileOperations::isDirectory(path)) {
			if ((file != ".") && (file != "..")) {
				result = scanDirectory(sha1sum, path);
			}
		}
		if (result.get()) {
			return result;
		}
	}
	return auto_ptr<File>(); // not found
}

auto_ptr<File> FilePool::scanFile(const string& sha1sum, const string& filename)
{
	Pool::iterator it = findInDatabase(filename);
	if (it == pool.end()) {
		// not in pool
		try {
			string sum;
			auto_ptr<File> file = calcSha1sum(filename, sum);
			time_t time = file->getModificationDate();
			pool.insert(make_pair(sum, make_pair(time, filename)));
			if (sum == sha1sum) {
				return file;
			}
		} catch (FileException& e) {
			// ignore
		}
	} else {
		// already in pool
		assert(filename == it->second.second);
		try {
			auto_ptr<File> file(new File(filename));
			time_t time = file->getModificationDate();
			if (time == it->second.first) {
				// db is still up to date
				if (it->first == sha1sum) {
					return file;
				}
			} else {
				// db outdated
				SHA1 sha1;
				sha1.update(file->mmap(), file->getSize());
				string sum = sha1.hex_digest();
				pool.erase(it);
				pool.insert(make_pair(sum,
				                      make_pair(time, filename)));
				if (sum == sha1sum) {
					return file;
				}
			}
		} catch (FileException& e) {
			// error reading file, remove from db
			pool.erase(it);
		}
	}
	return auto_ptr<File>(); // not valid
}

FilePool::Pool::iterator FilePool::findInDatabase(const string& filename)
{
	for (Pool::iterator it = pool.begin(); it != pool.end(); ++it) {
		if (it->second.second == filename) {
			return it;
		}
	}
	return pool.end();
}

} // namespace openmsx
