// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"


File::File(const std::string &url, OpenMode mode)
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
		file = new LocalFile(name, mode);
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

byte* File::mmap(bool writeBack)
{
	return file->mmap(writeBack);
}

void File::munmap()
{
	file->munmap();
}

int File::getSize()
{
	return file->getSize();
}

void File::seek(int pos)
{
	file->seek(pos);
}

int File::getPos()
{
	return file->getPos();
}

const std::string File::getURL() const
{
	return file->getURL();
}

const std::string File::getLocalName() const
{
	return file->getLocalName();
}
