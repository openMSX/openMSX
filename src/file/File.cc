// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"


namespace openmsx {

File::File(const string &url, OpenMode mode) throw(FileException)
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


void File::read(byte* buffer, unsigned num) throw(FileException)
{
	file->read(buffer, num);
}

void File::write(const byte* buffer, unsigned num) throw(FileException)
{
	file->write(buffer, num);
}

byte* File::mmap(bool writeBack) throw(FileException)
{
	return file->mmap(writeBack);
}

void File::munmap() throw(FileException)
{
	file->munmap();
}

unsigned File::getSize() throw(FileException)
{
	return file->getSize();
}

void File::seek(unsigned pos) throw(FileException)
{
	file->seek(pos);
}

unsigned File::getPos() throw(FileException)
{
	return file->getPos();
}

void File::truncate(unsigned size) throw(FileException)
{
	return file->truncate(size);
}

const string File::getURL() const throw(FileException)
{
	return file->getURL();
}

const string File::getLocalName() const throw(FileException)
{
	return file->getLocalName();
}

bool File::isReadOnly() const throw(FileException)
{
	return file->isReadOnly();
}

} // namespace openmsx
