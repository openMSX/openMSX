#include "HttpFile.hh"
#include "FileManager.hh"

HttpFile::HttpFile(const std::string& uri, byte mode)
:File(mode)
{
}

HttpFile::~HttpFile()
{
}

bool HttpFile::head()
{
	assert(false);
	return false;
}

void HttpFile::fetch()
{
	assert(false);
}

byte* HttpFile::getData(size_t& len)
{
	assert(false);
}

void HttpFile::setData(byte* data, size_t len)
{
	throw FileManagerException("HttpFile::setData: HTTP is readonly");
}

void HttpFile::put()
{
	throw FileManagerException("HttpFile::setData: HTTP is readonly");
}
