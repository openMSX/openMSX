// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"
#include "MSXConfig.hh"


File::File(const std::string &context, const std::string &url_, int options)
{
	std::string url(url_);
	unsigned int pos = url.find("://");
	if (pos == std::string::npos) {
		// no protocol specified
		url = context + url;
		pos = url.find("://");
	}
	
	std::string protocol, name;
	if (pos == std::string::npos) {
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

File::~File()
{
	delete file;
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
