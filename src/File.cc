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
		return new HttpFile(uri);
	}
	else if ((scheme == "file") || (scheme == ""))
	{
		return new LocalFile(uri);
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
{
}

File::~File()
{
}
