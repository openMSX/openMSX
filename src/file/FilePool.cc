// $Id$

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cstdio>
#include <alloca.h>
#include "sha1.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "FilePool.hh"
#include "ReadDir.hh"

using std::ifstream;
using std::ofstream;
using std::ostringstream;
using std::setfill;
using std::setw;
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

const char* const days[7] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat" ,"Sun"
};

const char* const months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static bool parse(const string& line, string& sha1, time_t& time, string& filename)
{
	char sum[41];
	char day[4];
	char month[4];
	char* file = static_cast<char*>(alloca(line.length()));
	struct tm tm;
	int items = sscanf(line.c_str(), "%40[0-9a-f]  %3s %3s %2u %2u:%2u:%2u %4u  %s",
	                   sum, day, month, &tm.tm_mday, &tm.tm_hour,
	                   &tm.tm_min, &tm.tm_sec, &tm.tm_year, file);
	sha1 = sum;
	filename = file;
	tm.tm_year -= 1900;
	tm.tm_isdst = -1;
	tm.tm_mon = -1;
	for (int i = 0; i < 12; ++i) {
		if (strcmp(month, months[i]) == 0) {
			tm.tm_mon = i;
			break;
		}
	}
	time = mktime(&tm);
	return ((items == 9) && (tm.tm_mon != -1) &&
	        (time != static_cast<time_t>(-1)));
}

static string toString(time_t time)
{
	struct tm* tm;
	tm = localtime(&time);
	ostringstream sstr;
	sstr << setfill('0')
	     << days[tm->tm_wday] << " "
	     << months[tm->tm_mon] << " "
	     << setw(2) << tm->tm_mday << " "
	     << setw(2) << tm->tm_hour << ":"
	     << setw(2) << tm->tm_min << ":"
	     << setw(2) << tm->tm_sec << " "
	     << setw(4) << (tm->tm_year + 1900);
	return sstr.str();
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
		     << toString(it->second.first) << "  "
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
