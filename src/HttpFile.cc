// $Id$

#include "HttpFile.hh"
#include "FileManager.hh"

#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
extern int errno;
#include <sys/types.h>
#include <sys/socket.h>

// please note that HttpFile.cc as it currently
// is implemented contains stuff not portable to
// windoze, for ideas how to fix it, look for example
// in nanohttp.c of libxml2 [http://xmlsoft.org]
// it should not be hard

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
	int s;
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		std::ostringstream s;
		s << "HttpFile::head: socket:" << strerror(errno);
		throw FileManagerException(s.str());
	}
	int status;
	if ((status = fcntl(s, F_GETFL, 0)) != -1) {
		status |= O_NONBLOCK;
		status = fcntl(s, F_SETFL, status);
	}
	else
	{
		std::ostringstream s;
		s << "HttpFile::head: fcntl:" << strerror(errno);
		throw FileManagerException(s.str());
	}
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
