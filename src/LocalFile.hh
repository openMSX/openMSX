#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include <string>

#include "File.hh"

class LocalFile: public File
{
public:
	LocalFile(const std::string& uri);
	virtual ~LocalFile();

	virtual void fetch();
	virtual byte* getData(size_t& len);

private:
};

#endif
