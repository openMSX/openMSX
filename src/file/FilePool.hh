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
	typedef multimap<string, pair<time_t, string> > Pool;
	typedef map<string, Pool> Database;

	FilePool();
	~FilePool();
	
	void readSha1sums(const string& directory, Pool& pool);
	void writeSha1sums(const string& directory, const Pool& pool);
	string getFromPool(const string& sha1sum, const string& directory,
	                   Pool& pool);
	string scanDirectory(const string& sha1sum, const string& directory,
                             Pool& pool);
	void calcSha1sum(const string& filename, time_t& time, string& sum);
	Pool::iterator findInDatabase(const string& filename, Pool& pool);

	Database database;
};

} // namespace openmsx

#endif
