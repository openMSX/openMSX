#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include <string>

#include "File.hh"

class LocalFile: public File
{
public:
	LocalFile(const std::string& uri, byte mode);
	virtual ~LocalFile();

	virtual void fetch();
	virtual void put();
	virtual bool head();
	virtual byte* getData(size_t& len);
	virtual void setData(byte* data, size_t len);

private:
	std::string path;
	size_t len;
};

#endif
