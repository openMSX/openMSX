// $Id$

#ifndef __HTTPFILE_HH__
#define __HTTPFILE_HH__

#include <string>

#include "File.hh"

class HttpFile: public File
{
public:
	HttpFile(const std::string& uri, byte mode);
	virtual ~HttpFile();

	virtual bool head();
	virtual void fetch();
	virtual void put();
	virtual byte* getData(size_t& len);
	virtual void setData(byte* data, size_t len);

private:
};

#endif
