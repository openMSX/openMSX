#ifndef __HTTPFILE_HH__
#define __HTTPFILE_HH__

#include <string>

#include "File.hh"

class HttpFile: public File
{
public:
	HttpFile(const std::string& uri);
	virtual ~HttpFile();

	virtual void fetch();
	virtual byte* getData(size_t& len);

private:
};

#endif
