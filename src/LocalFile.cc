#include "LocalFile.hh"
#include "FileType.hh"
#include "FileManager.hh"

#include <cstdlib>
#include <libxml/uri.h>

LocalFile::LocalFile(const std::string& uri, byte mode)
:File(mode)
{
	xmlURIPtr urip = xmlParseURI(uri.c_str());
	path = urip->path;
	if (path.length()>1)
	{
		if (path[0]=='~')
		{
			if (path[1]=='/')
			{
				path = std::string(getenv("HOME")) + path;
			}
			else
			{
				assert(false);
			}
		}
	}
	xmlFreeURI(urip);
}

LocalFile::~LocalFile()
{
}

bool LocalFile::head()
{
	IFILETYPE file(path.c_str());
	return file.fail();
}

void LocalFile::fetch()
{
	IFILETYPE file(path.c_str());
	file.seekg(0,std::ios::end);
	len = file.tellg();
	file.seekg(0,std::ios::beg);
	file.read(buffer, len);
	file.close();
}

void LocalFile::put()
{
	if (mode&(write|prefer)>0)
	{
		OFILETYPE file(path.c_str(), std::ios::out|std::ios::trunc);
		if (file.fail() && (mode&prefer == 0))
		{
			throw FileManagerException("LocalFile::put: fail to put a file, while 'write'-mode is asked");
		}
		file.write(buffer, len);
		file.close();
	}
	else
	{
		throw FileManagerException("LocalFile::put: trying to 'put' a file while mode doesn't contain 'write' or 'prefer'");
	}
}

byte* LocalFile::getData(size_t& len)
{
	return buffer;
}

void LocalFile::setData(byte* data, size_t len)
{
	memcpy(buffer, data, len);
}
