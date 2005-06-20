// $Id$

#include "probed_defs.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef	HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "LocalFile.hh"
#include "FileOperations.hh"
#include "FileException.hh"

using std::string;

namespace openmsx {

LocalFile::LocalFile(const string& filename_, OpenMode mode)
	: filename(FileOperations::expandTilde(filename_)),
	  readOnly(false)
{
	PRT_DEBUG("LocalFile: " << filename);

	if (mode == SAVE_PERSISTENT) {
		string::size_type pos = filename.find_last_of('/');
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

void LocalFile::read(byte* buffer, unsigned num)
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

void LocalFile::write(const byte* buffer, unsigned num)
{
	fwrite(buffer, 1, num, file);
	if (ferror(file)) {
		throw FileException("Error writing file");
	}
}

#ifdef HAVE_MMAP
byte* LocalFile::mmap(bool writeBack)
{
	if (!mmem) {
		int flags = writeBack ? MAP_SHARED : MAP_PRIVATE;
		mmem = (byte*)::mmap(0, getSize(), PROT_READ | PROT_WRITE,
		                     flags, fileno(file), 0);
		if (mmem == MAP_FAILED) {
			throw FileException("Error mmapping file");
		}
	}
	return mmem;
}

void LocalFile::munmap()
{
	if (mmem) {
		::munmap(mmem, getSize());
		mmem = NULL;
	}
}
#endif

unsigned LocalFile::getSize()
{
	long pos = ftell(file);
	fseek(file, 0, SEEK_END);
	unsigned size = (unsigned)ftell(file);
	fseek(file, pos, SEEK_SET);
	return size;
}

void LocalFile::seek(unsigned pos)
{
	fseek(file, pos, SEEK_SET);
	if (ferror(file)) {
		throw FileException("Error seeking file");
	}
}

unsigned LocalFile::getPos()
{
	return (unsigned)ftell(file);
}

#ifdef HAVE_FTRUNCATE
void LocalFile::truncate(unsigned size)
{
	int fd = fileno(file);
	if (ftruncate(fd, size)) {
		throw FileException("Error truncating file");
	}
}
#endif

void LocalFile::flush()
{
	fflush(file);
}

const string LocalFile::getURL() const
{
	static const string prefix("file://");
	return prefix + filename;
}

const string LocalFile::getLocalName()
{
	return filename;
}

bool LocalFile::isReadOnly() const
{
	return readOnly;
}

time_t LocalFile::getModificationDate()
{
	struct stat st;
	if (fstat(fileno(file), &st)) {
		throw FileException("Cannot stat file");
	}
	return st.st_mtime;
}

} // namespace openmsx
