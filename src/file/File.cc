// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"
#include "MSXConfig.hh"


// class FileContext

FileContext::FileContext(const std::string &path)
{
	paths.push_back(path);
}

FileContext::FileContext(bool isSystem)
{
	if (isSystem) {
		// initialize as system context
		paths.push_back("~/.openMSX/");
		paths.push_back("~/openMSX/");
		paths.push_back("/usr/local/etc/openMSX/");
		paths.push_back("/etc/openMSX/");
	} else {
		// initialize as user context
		// TODO
	}
}

const FileContext& FileContext::getSystemContext()
{
	static FileContext systemContext(true);
	return systemContext;
}

const FileContext& FileContext::getUserContext()
{
	static FileContext userContext(false);
	return userContext;
}


// class File

File::File(const FileContext &context, const std::string &url, int options)
{
	if (url.find("://") != std::string::npos) {
		// protocol specified, don't use SearchPath
		open(url, options);
	} else {
		std::list<std::string>::const_iterator it;
		for (it = context.paths.begin(); 
		     it != context.paths.end(); it++) {
			try {
				open(*it + url, options);
				return;
			} catch (FileException &e) {
				// try next
			}
		}
		throw FileException("Error opening file " + url);
	}
}

File::~File()
{
	delete file;
}


void File::open(const std::string &url, int options)
{
	std::string protocol, name;
	unsigned int pos = url.find("://");
	if (pos == std::string::npos) {
		// no explicit protocol, take "file"
		protocol = "file";
		name = url;
	} else {
		protocol = url.substr(0, pos);
		name = url.substr(pos + 3);
	}
	
	PRT_DEBUG("File: " << protocol << "://" << name);
	if (protocol == "file") {
		file = new LocalFile(name, options);
	} else {
		PRT_ERROR("Unsupported protocol: " << protocol);
	}
}

void File::read(byte* buffer, int num)
{
	file->read(buffer, num);
}

void File::write(const byte* buffer, int num)
{
	file->write(buffer, num);
}

byte* File::mmap(bool write)
{
	return file->mmap(write);
}

void File::munmap()
{
	file->munmap();
}

int File::size()
{
	return file->size();
}

void File::seek(int pos)
{
	file->seek(pos);
}

int File::pos()
{
	return file->pos();
}

const std::string& File::getLocalName()
{
	return file->getLocalName();
}
