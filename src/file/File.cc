// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"


namespace openmsx {

File::File(const string &url, OpenMode mode)
{
	string protocol, name;
	unsigned pos = url.find("://");
	if (pos == string::npos) {
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
		throw FatalError("Unsupported protocol: " + protocol);
	}

	if (((pos = name.rfind(".gz")) != string::npos) &&
	    (pos == (name.size() - 3))) {
		file = new GZFileAdapter(file);
	} else
	if (((pos = name.rfind(".zip")) != string::npos) &&
	    (pos == (name.size() - 4))) {
		file = new ZipFileAdapter(file);
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

const string File::getURL() const
{
	return file->getURL();
}

const string File::getLocalName() const
{
	return file->getLocalName();
}

bool File::isReadOnly() const
{
	return file->isReadOnly();
}

} // namespace openmsx
