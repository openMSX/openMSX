// $Id$

#ifndef __FILEPOOL_HH__
#define __FILEPOOL_HH__

#include <string>
#include <map>
#include <time.h>

using std::string;
using std::multimap;
using std::pair;

namespace openmsx {

class Config;

class FilePool
{
public:
	static FilePool& instance();
	
	string getFile(const string& sha1sum);

private:
	typedef multimap<string, pair<time_t, string> > Database;

	FilePool();
	~FilePool();
	
	void readSha1sums(const string& directory);
	void writeSha1sums(const string& directory);
	void calcSha1sum(const string& filename, time_t& time, string& sum);
	Database::iterator findInDatabase(const string& filename);

	string pool;
	Database database;
};

} // namespace openmsx

#endif
