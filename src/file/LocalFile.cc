// $Id$

#include "config.h"
#include <unistd.h>
#ifdef	HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "LocalFile.hh"
#include "FileOperations.hh"


namespace openmsx {

LocalFile::LocalFile(const string& filename_, OpenMode mode)
	throw(FileException)
	: filename(FileOperations::expandTilde(filename_)),
	  readOnly(false)
{
	PRT_DEBUG("LocalFile: " << filename);

	if (mode == SAVE_PERSISTENT) {
		unsigned pos = filename.find_last_of('/');
		if (pos != string::npos) {
			FileOperations::mkdirp(filename.substr(0, pos));
		}
	}

	const string name = FileOperations::getNativePath(filename);
	if ((mode == SAVE_PERSISTENT) || (mode == TRUNCATE)) {
		// open file read/write truncated
		file = fopen(name.c_str(), "wb+");
	} else if (mode == CREATE) {
		// open file read/write
		file = fopen(name.c_str(), "rb+");
		if (!file) {
			// create if it didn't exist yet
			file = fopen(name.c_str(), "wb+");
		}
	} else {
		// open file read/write
		file = fopen(name.c_str(), "rb+");
		if (!file) {
			// if that fails try read only
			file = fopen(name.c_str(), "rb");
			readOnly = true;
		}
	}
	if (!file) {
		throw FileException("Error opening file " + filename);
	}
}

LocalFile::~LocalFile()
{
	munmap();
	fclose(file);
}

void LocalFile::read(byte* buffer, unsigned num) throw(FileException)
{
	long pos = ftell(file);
	fseek(file, 0, SEEK_END);
	unsigned size = (unsigned)ftell(file);
	fseek(file, pos, SEEK_SET);
	if ((pos + num) > size) {
		throw FileException("Read beyond end of file");
	}

	fread(buffer, 1, num, file);
	if (ferror(file)) {
		throw FileException("Error reading file");
	}
}

void LocalFile::write(const byte* buffer, unsigned num) throw(FileException)
{
	fwrite(buffer, 1, num, file);
	if (ferror(file)) {
		throw FileException("Error writing file");
	}
}

#ifdef HAVE_MMAP
byte* LocalFile::mmap(bool writeBack) throw(FileException)
{
	if (!mmem) {
		int flags = writeBack ? MAP_SHARED : MAP_PRIVATE;
		mmem = (byte*)::mmap(0, getSize(), PROT_READ | PROT_WRITE,
		                     flags, fileno(file), 0);
		if ((int)mmem == -1) {
			throw FileException("Error mmapping file");
		}
	}
	return mmem;
}

void LocalFile::munmap() throw()
{
	if (mmem) {
		::munmap(mmem, getSize());
		mmem = NULL;
	}
}
#endif

unsigned LocalFile::getSize() throw()
{
	long pos = ftell(file);
	fseek(file, 0, SEEK_END);
	unsigned size = (unsigned)ftell(file);
	fseek(file, pos, SEEK_SET);
	return size;
}

void LocalFile::seek(unsigned pos) throw(FileException)
{
	fseek(file, pos, SEEK_SET);
	if (ferror(file)) {
		throw FileException("Error seeking file");
	}
}

unsigned LocalFile::getPos() throw()
{
	return (unsigned)ftell(file);
}

#ifdef HAVE_FTRUNCATE
void LocalFile::truncate(unsigned size) throw(FileException)
{
	int fd = fileno(file);
	if (ftruncate(fd, size)) {
		throw FileException("Error truncating file");
	}
}
#endif

const string LocalFile::getURL() const throw()
{
	static const string prefix("file://");
	return prefix + filename;
}

const string LocalFile::getLocalName() throw()
{
	return filename;
}

bool LocalFile::isReadOnly() const throw()
{
	return readOnly;
}

} // namespace openmsx
