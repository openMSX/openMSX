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

FilePool::FilePool()
{
	try {
		Config* config = SettingsConfig::instance().getConfigById("RomPool");
		pool = FileOperations::expandTilde(config->getParameter("directory"));
		readSha1sums(pool);
	} catch (ConfigException& e) {
		// No RomPool section
	}
}

FilePool::~FilePool()
{
	if (!pool.empty()) {
		writeSha1sums(pool);
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

	return (line.find_first_not_of("0123456789abcdef") == string::npos) &&
	       (time != static_cast<time_t>(-1));
}

void FilePool::readSha1sums(const string& directory)
{
	string filename = directory + "/.filecache";
	ifstream file(filename.c_str());
	if (!file.is_open()) {
		return;
	}
	while (file.good()) {
		string line;
		getline(file, line);

		string sum;
		string filename;
		time_t time;
		if (parse(line, sum, time, filename)) {
			database.insert(make_pair(sum, make_pair(time, filename)));
		}
	}
}

void FilePool::writeSha1sums(const string& directory)
{
	string filename = directory + "/.filecache";
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		return;
	}
	for (Database::const_iterator it = database.begin();
	     it != database.end(); ++it) {
		file << it->first << "  "
		     << Date::toString(it->second.first) << "  "
		     << it->second.second
		     << endl;
	}
}


string FilePool::getFile(const string& sha1sum)
{
	pair<Database::iterator, Database::iterator> bound =
		database.equal_range(sha1sum);
	for (Database::iterator it = bound.first; it != bound.second; ++it) {
		time_t& time = it->second.first;
		const string& filename = it->second.second;
		try {
			time_t newTime;
			string newSum;
			calcSha1sum(filename, newTime, newSum);
			if (newSum == sha1sum) {
				time = newTime;
				return filename;
			} else {
				// did not match, update db with new sum
				database.erase(it);
				database.insert(make_pair(newSum,
				                 make_pair(newTime, filename)));
			}
		} catch (FileException& e) {
			// error reading file, remove from db
			database.erase(it);
		}
	}
	
	ReadDir dir(pool);
	struct dirent* d;
	while ((d = dir.getEntry())) {
		string filename = pool + '/' + d->d_name;
		Database::iterator it = findInDatabase(filename);
		if (it == database.end()) {
			// not in database
			try {
				time_t time;
				string sum;
				calcSha1sum(filename, time, sum);
				database.insert(make_pair(sum,
				                 make_pair(time, filename)));
				if (sum == sha1sum) {
					return filename;
				}
			} catch (FileException& e) {
				// ignore
			}
		} else {
			// already in database
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
					database.erase(it);
					database.insert(make_pair(sum,
					                 make_pair(time, filename)));
					if (sum == sha1sum) {
						return filename;
					}
				}
			} catch (FileException& e) {
				// error reading file, remove from db
				database.erase(it);
			}
		}
	}
	return "";
}

void FilePool::calcSha1sum(const string& filename, time_t& time, string& sum)
{
	File file(filename);
	time = file.getModificationDate();
	byte* data = file.mmap();
	SHA1 sha1;
	sha1.update(data, file.getSize());
	sha1.finalize();
	sum = sha1.hex_digest();
}

FilePool::Database::iterator FilePool::findInDatabase(const string& filename)
{
	for (Database::iterator it = database.begin(); it != database.end(); ++it) {
		if (it->second.second == filename) {
			return it;
		}
	}
	return database.end();
}

} // namespace openmsx
