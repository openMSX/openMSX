// $Id$

#include <fstream>
#include <cassert>
#include "sha1.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "FilePool.hh"
#include "ReadDir.hh"
#include "Date.hh"

using std::ifstream;
using std::ofstream;
using std::make_pair;
using std::endl;

namespace openmsx {

const char* const FILE_CACHE = ".filecache";

FilePool::FilePool()
{
	try {
		Config* config = SettingsConfig::instance().getConfigById("RomPool");
		XMLElement::Children dirs;
		config->getChildren("directory", dirs);
		for (XMLElement::Children::const_iterator it = dirs.begin();
		     it != dirs.end(); ++it) {
			string dir = FileOperations::expandTilde((*it)->getData());
			readSha1sums(dir, database[dir]);
		}
	} catch (ConfigException& e) {
		// No RomPool section
	}
}

FilePool::~FilePool()
{
	for (Database::const_iterator it = database.begin();
	     it != database.end(); ++it) {
		writeSha1sums(it->first, it->second);
	}
}

FilePool& FilePool::instance()
{
	static FilePool oneInstance;
	return oneInstance;
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

void FilePool::readSha1sums(const string& directory, Pool& pool)
{
	string filename = directory + '/' + FILE_CACHE;
	ifstream file(filename.c_str());
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

void FilePool::writeSha1sums(const string& directory, const Pool& pool)
{
	string filename = directory + '/' + FILE_CACHE;
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		return;
	}
	for (Pool::const_iterator it = pool.begin(); it != pool.end(); ++it) {
		file << it->first << "  "
		     << Date::toString(it->second.first) << "  "
		     << it->second.second
		     << endl;
	}
}

string FilePool::getFile(const string& sha1sum)
{
	for (Database::iterator it = database.begin();
	     it != database.end(); ++it) {
		string filename = getFromPool(sha1sum, it->first, it->second);
		if (!filename.empty()) {
			return filename;
		}
	}

	for (Database::iterator it = database.begin();
	     it != database.end(); ++it) {
		string filename = scanDirectory(sha1sum, it->first, it->second);
		if (!filename.empty()) {
			return filename;
		}
	}
	
	return "";
}

string FilePool::getFromPool(const string& sha1sum, const string& directory,
                             Pool& pool)
{
	pair<Pool::iterator, Pool::iterator> bound = pool.equal_range(sha1sum);
	Pool::iterator it = bound.first;
	while (it != bound.second) {
		time_t& time = it->second.first;
		const string& filename = it->second.second;
		string full_name = directory + '/' + filename;
		try {
			time_t newTime;
			string newSum;
			calcSha1sum(full_name, newTime, newSum);
			if (newSum == sha1sum) {
				time = newTime;
				return full_name;
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
	return "";
}

string FilePool::scanDirectory(const string& sha1sum, const string& directory,
                               Pool& pool)
{
	ReadDir dir(directory);
	while (dirent* d = dir.getEntry()) {
		string filename = d->d_name;
		string full_name = directory + '/' + filename;
		if (!FileOperations::isRegularFile(full_name)) {
			continue;
		}
		Pool::iterator it = findInDatabase(filename, pool);
		if (it == pool.end()) {
			// not in pool
			try {
				time_t time;
				string sum;
				calcSha1sum(full_name, time, sum);
				pool.insert(make_pair(sum,
				                 make_pair(time, filename)));
				if (sum == sha1sum) {
					return full_name;
				}
			} catch (FileException& e) {
				// ignore
			}
		} else {
			// already in pool
			assert(filename == it->second.second);
			try {
				File file(full_name);
				if (file.getModificationDate() == it->second.first) {
					// db is still up to date
					if (it->first == sha1sum) {
						return full_name;
					}
				} else {
					// db outdated
					time_t time;
					string sum;
					calcSha1sum(full_name, time, sum);
					pool.erase(it);
					pool.insert(make_pair(sum,
					                 make_pair(time, filename)));
					if (sum == sha1sum) {
						return full_name;
					}
				}
			} catch (FileException& e) {
				// error reading file, remove from db
				pool.erase(it);
			}
		}
	}
	return "";
}

void FilePool::calcSha1sum(const string& filename, time_t& time, string& sum)
{
	PRT_DEBUG("FilePool: calculating SHA1 for " << filename);

	File file(filename);
	time = file.getModificationDate();
	byte* data = file.mmap();
	SHA1 sha1;
	sha1.update(data, file.getSize());
	sha1.finalize();
	sum = sha1.hex_digest();
}

FilePool::Pool::iterator FilePool::findInDatabase(const string& filename,
                                                  Pool& pool)
{
	for (Pool::iterator it = pool.begin(); it != pool.end(); ++it) {
		if (it->second.second == filename) {
			return it;
		}
	}
	return pool.end();
}

} // namespace openmsx
