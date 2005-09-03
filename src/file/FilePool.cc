// $Id$

#include <fstream>
#include <cassert>
#include "sha1.hh"
#include "SettingsConfig.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "FilePool.hh"
#include "ReadDir.hh"
#include "Date.hh"

using std::endl;
using std::ifstream;
using std::make_pair;
using std::ofstream;
using std::pair;
using std::string;
using std::vector;

namespace openmsx {

const char* const FILE_CACHE = "/.filecache";

FilePool::FilePool(SettingsConfig& settingsConfig)
{
	const XMLElement* config = settingsConfig.findChild("RomPool");
	if (config) {
		XMLElement::Children dirs;
		config->getChildren("directory", dirs);
		for (XMLElement::Children::const_iterator it = dirs.begin();
		     it != dirs.end(); ++it) {
			string dir = FileOperations::expandTilde((*it)->getData());
			directories.push_back(dir);
		}
	}
	SystemFileContext context;
	const vector<string>& paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		string dir = *it + "/systemroms";
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
	       (time != static_cast<time_t>(-1));
}

static void calcSha1sum(const string& filename, time_t& time, string& sum)
{
	File file(filename);
	time = file.getModificationDate();
	byte* data = file.mmap();
	SHA1 sha1;
	sha1.update(data, file.getSize());
	sum = sha1.hex_digest();
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

string FilePool::getFile(const string& sha1sum)
{
	string filename = getFromPool(sha1sum);
	if (!filename.empty()) {
		return filename;
	}

	for (Directories::const_iterator it = directories.begin();
	     it != directories.end(); ++it) {
		string filename = scanDirectory(sha1sum, *it);
		if (!filename.empty()) {
			return filename;
		}
	}

	return string(); // not found
}

string FilePool::getFromPool(const string& sha1sum)
{
	pair<Pool::iterator, Pool::iterator> bound = pool.equal_range(sha1sum);
	Pool::iterator it = bound.first;
	while (it != bound.second) {
		time_t& time = it->second.first;
		const string& filename = it->second.second;
		try {
			time_t newTime;
			string newSum;
			calcSha1sum(filename, newTime, newSum);
			if (newSum == sha1sum) {
				time = newTime;
				return filename;
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
	return string();
}

string FilePool::scanDirectory(const string& sha1sum, const string& directory)
{
	ReadDir dir(directory);
	while (dirent* d = dir.getEntry()) {
		string filename = directory + '/' + d->d_name;
		if (!FileOperations::isRegularFile(filename)) {
			continue;
		}
		Pool::iterator it = findInDatabase(filename);
		if (it == pool.end()) {
			// not in pool
			try {
				time_t time;
				string sum;
				calcSha1sum(filename, time, sum);
				pool.insert(make_pair(sum,
				                 make_pair(time, filename)));
				if (sum == sha1sum) {
					return filename;
				}
			} catch (FileException& e) {
				// ignore
			}
		} else {
			// already in pool
			assert(filename == it->second.second);
			try {
				File file(filename);
				if (file.getModificationDate() == it->second.first) {
					// db is still up to date
					if (it->first == sha1sum) {
						return filename;
					}
				} else {
					// db outdated
					time_t time;
					string sum;
					calcSha1sum(filename, time, sum);
					pool.erase(it);
					pool.insert(make_pair(sum,
					                 make_pair(time, filename)));
					if (sum == sha1sum) {
						return filename;
					}
				}
			} catch (FileException& e) {
				// error reading file, remove from db
				pool.erase(it);
			}
		}
	}
	return string();
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
