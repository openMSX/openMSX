#include <sstream>

#include <cstdio>

#include <libxml/uri.h>

#include "FileManager.hh"
#include "File.hh"
#include "HttpFile.hh"
#include "LocalFile.hh"

File* File::createFile(const std::string& uri, byte mode)
{
	xmlURIPtr urip = xmlParseURI(uri.c_str());
	if (urip == NULL)
	{
		std::ostringstream o;
		o << "failed to parse URI: " << uri;
		throw FileManagerException(o.str());
	}
	File* file = NULL;
	std::string scheme(urip->scheme);
	if (scheme == "http")
	{
		file = new HttpFile(uri, mode);
	}
	else if ((scheme == "file") || (scheme == ""))
	{
		file = new LocalFile(uri, mode);
	}
	else
	{
		xmlFreeURI(urip);
		std::ostringstream o;
		o << "no code yet to handle protocol: " << scheme;
		throw FileManagerException(o.str());
	}
	xmlFreeURI(urip);
	return file;
}

File::File()
:buffer(NULL), mode(read|write|prefer)
{
}

File::File(byte mode_)
:buffer(NULL), mode(mode_)
{
}

File::~File()
{
	delete[] buffer;
}
