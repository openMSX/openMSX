// $Id$

#include "systemfuncs.hh"
#include "unistdp.hh"
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <iostream>
#endif
#include "LocalFile.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "PreCacheFile.hh"
#include "StringOp.hh"
#include <cstring> // for strchr
#include <cassert>

using std::string;

namespace openmsx {

LocalFile::LocalFile(const string& filename_, File::OpenMode mode)
	: filename(FileOperations::expandTilde(filename_))
#ifdef _WIN32
	, hMmap(NULL)
#endif
	, readOnly(false)
{
	PRT_DEBUG("LocalFile: " << filename);

	if (mode == File::SAVE_PERSISTENT) {
		string::size_type pos = filename.find_last_of('/');
		if (pos != string::npos) {
			FileOperations::mkdirp(filename.substr(0, pos));
		}
	}

	const string name = FileOperations::getNativePath(filename);
	if ((mode == File::SAVE_PERSISTENT) || (mode == File::TRUNCATE)) {
		// open file read/write truncated
		file = FileOperations::openFile(name, "wb+");
	} else if (mode == File::CREATE) {
		// open file read/write
		file = FileOperations::openFile(name, "rb+");
		if (!file) {
			// create if it didn't exist yet
			file = FileOperations::openFile(name, "wb+");
		}
	} else {
		// open file read/write
		file = FileOperations::openFile(name, "rb+");
		if (!file) {
			// if that fails try read only
			file = FileOperations::openFile(name, "rb");
			readOnly = true;
		}
	}
	if (!file) {
		throw FileException("Error opening file \"" + filename + "\"");
	}

	if (mode == File::PRE_CACHE) {
		cache.reset(new PreCacheFile(name));
	}
}

LocalFile::LocalFile(const std::string& filename, const char* mode)
{
	assert(strchr(mode, 'b'));
	const string name = FileOperations::getNativePath(filename);
	file = FileOperations::openFile(name, mode);
	if (!file) {
		throw FileException("Error opening file \"" + filename + "\"");
	}
}

LocalFile::~LocalFile()
{
	munmap();
	fclose(file);
}

void LocalFile::read(void* buffer, unsigned num)
{
	long pos = ftell(file);
	fseek(file, 0, SEEK_END);
	unsigned size = unsigned(ftell(file));
	fseek(file, pos, SEEK_SET);
	if ((pos + num) > size) {
		throw FileException("Read beyond end of file");
	}

	if (fread(buffer, 1, num, file) != num) {
		if (ferror(file)) {
			throw FileException("Error reading file");
		}
	}
}

void LocalFile::write(const void* buffer, unsigned num)
{
	if (fwrite(buffer, 1, num, file) != num) {
		if (ferror(file)) {
			throw FileException("Error writing file");
		}
	}
}

#ifdef _WIN32
byte* LocalFile::mmap()
{
	if (!mmem) {
		int fd = _fileno(file);
		if (fd == -1) {
			throw FileException("_fileno failed");
		}
		HANDLE hFile = reinterpret_cast<HANDLE>(_get_osfhandle(fd)); // No need to close
		if (hFile == INVALID_HANDLE_VALUE) {
			throw FileException("_get_osfhandle failed");
		}
		assert(!hMmap);
		hMmap = CreateFileMapping(hFile, NULL, PAGE_WRITECOPY, 0, 0, NULL);
		if (!hMmap) {
			throw FileException("CreateFileMapping failed: " +
				StringOp::toString(GetLastError()));
		}
		mmem = static_cast<byte*>(MapViewOfFile(hMmap, FILE_MAP_COPY, 0, 0, 0));
		if (!mmem) {
			DWORD gle = GetLastError();
			CloseHandle(hMmap);
			hMmap = NULL;
			throw FileException("MapViewOfFile failed: " +
				StringOp::toString(gle));
		}
	}
	return mmem;
}

void LocalFile::munmap()
{
	if (mmem) {
		// TODO: make this a valid failure path
		// When pages are dirty, UnmapViewOfFile is a save operation,
		// and that can fail. However, mummap is called from
		// the destructor, for which there is no expectation
		// that it will fail. So this area needs some work.
		// It is NOT an option to throw an exception (not even
		// FatalError).
		if (!UnmapViewOfFile(mmem)) {
			std::cerr << "UnmapViewOfFile failed: "
			          << StringOp::toString(GetLastError())
			          << std::endl;
		}
		mmem = NULL;
	}
	if (hMmap) {
		CloseHandle(hMmap);
		hMmap = NULL;
	}
}

#elif defined HAVE_MMAP
byte* LocalFile::mmap()
{
	if (!mmem) {
		mmem = static_cast<byte*>(
		          ::mmap(0, getSize(), PROT_READ | PROT_WRITE,
		                 MAP_PRIVATE, fileno(file), 0));
		// MAP_FAILED is #define'd using an old-style cast, we
		// have to redefine it ourselves to avoid a warning
		void* MY_MAP_FAILED = reinterpret_cast<void*>(-1);
		if (mmem == MY_MAP_FAILED) {
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
	unsigned size = unsigned(ftell(file));
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
	return unsigned(ftell(file));
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
	return filename;
}

const string LocalFile::getLocalReference()
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
