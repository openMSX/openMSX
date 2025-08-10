#include "systemfuncs.hh"
#include "unistdp.hh"
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <iostream>
#endif
#include "LocalFile.hh"
#include "FileException.hh"
#include "FileNotFoundException.hh"
#include "narrow.hh"
#include "one_of.hh"

#include <bit>
#include <cstring> // for strchr, strerror
#include <cerrno>
#include <cassert>
#include <memory>

namespace openmsx {

LocalFile::LocalFile(std::string filename_, File::OpenMode mode)
	: filename(std::move(filename_))
{
	if (mode == File::OpenMode::SAVE_PERSISTENT) {
		if (auto pos = filename.find_last_of('/'); pos != std::string::npos) {
			FileOperations::mkdirp(filename.substr(0, pos));
		}
	}

	const std::string& name = FileOperations::getNativePath(filename);
	if (mode == one_of(File::OpenMode::SAVE_PERSISTENT, File::OpenMode::TRUNCATE)) {
		// open file read/write truncated
		file = FileOperations::openFile(name, "wb+");
	} else if (mode == File::OpenMode::CREATE) {
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
		int err = errno;
		if (err == ENOENT) {
			throw FileNotFoundException(
				"File \"", filename, "\" not found");
		} else {
			throw FileException(
				"Error opening file \"", filename, "\": ",
				strerror(err));
		}
	}
	(void)getSize(); // query filesize, but ignore result
}

LocalFile::LocalFile(std::string filename_, const char* mode)
	: filename(std::move(filename_))
{
	assert(strchr(mode, 'b'));
	const std::string name = FileOperations::getNativePath(filename);
	file = FileOperations::openFile(name, mode);
	if (!file) {
		throw FileException("Error opening file \"", filename, '"');
	}
	(void)getSize(); // query filesize, but ignore result
}

LocalFile::~LocalFile()
{
	munmap();
}

void LocalFile::preCacheFile()
{
	cache.emplace(FileOperations::getNativePath(filename));
}

void LocalFile::read(std::span<uint8_t> buffer)
{
	if (fread(buffer.data(), 1, buffer.size(), file.get()) != buffer.size()) {
		if (ferror(file.get())) {
			throw FileException("Error reading file");
		}
		if (feof(file.get())) {
			throw FileException("Read beyond end of file");
		}
	}
}

void LocalFile::write(std::span<const uint8_t> buffer)
{
	if (fwrite(buffer.data(), 1, buffer.size(), file.get()) != buffer.size()) {
		if (ferror(file.get())) {
			throw FileException("Error writing file");
		}
	}
}

#ifdef _WIN32
std::span<const uint8_t> LocalFile::mmap()
{
	size_t size = getSize();
	if (size == 0) return {static_cast<uint8_t*>(nullptr), size};

	if (!mmem) {
		int fd = _fileno(file.get());
		if (fd == -1) {
			throw FileException("_fileno failed");
		}
		auto hFile = reinterpret_cast<HANDLE>(_get_osfhandle(fd)); // No need to close
		if (hFile == INVALID_HANDLE_VALUE) {
			throw FileException("_get_osfhandle failed");
		}
		assert(!hMmap);
		hMmap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, 0, nullptr);
		if (!hMmap) {
			throw FileException(
				"CreateFileMapping failed: ", GetLastError());
		}
		mmem = static_cast<uint8_t*>(MapViewOfFile(hMmap, FILE_MAP_COPY, 0, 0, 0));
		if (!mmem) {
			DWORD gle = GetLastError();
			CloseHandle(hMmap);
			hMmap = nullptr;
			throw FileException("MapViewOfFile failed: ", gle);
		}
	}
	return {mmem, size};
}

void LocalFile::munmap()
{
	if (mmem) {
		// TODO: make this a valid failure path
		// When pages are dirty, UnmapViewOfFile is a save operation,
		// and that can fail. However, munmap is called from
		// the destructor, for which there is no expectation
		// that it will fail. So this area needs some work.
		// It is NOT an option to throw an exception (not even
		// FatalError).
		if (!UnmapViewOfFile(mmem)) {
			std::cerr << "UnmapViewOfFile failed: "
			          << GetLastError()
			          << '\n';
		}
		mmem = nullptr;
	}
	if (hMmap) {
		CloseHandle(hMmap);
		hMmap = nullptr;
	}
}

#elif HAVE_MMAP
std::span<const uint8_t> LocalFile::mmap()
{
	size_t size = getSize();
	if (size == 0) return {static_cast<uint8_t*>(nullptr), size};

	if (!mmem) {
		mmem = static_cast<uint8_t*>(
		          ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
		                 MAP_PRIVATE, fileno(file.get()), 0));
		// MAP_FAILED is #define'd using an old-style cast, we
		// have to redefine it ourselves to avoid a warning
		auto* MY_MAP_FAILED = std::bit_cast<void*>(intptr_t(-1));
		if (mmem == MY_MAP_FAILED) {
			throw FileException("Error mmapping file");
		}
	}
	return {mmem, size};
}

void LocalFile::munmap()
{
	if (mmem) {
		try {
			::munmap(mmem, getSize());
		} catch (FileException&) {
			// In theory getSize() could throw. Does that ever
			// happen in practice?
		}
		mmem = nullptr;
	}
}
#endif

size_t LocalFile::getSize()
{
#ifdef _WIN32
	// Normal fstat compiles but doesn't seem to be working the same
	// as on POSIX, regarding size support.
	struct _stat64 st;
	int ret = _fstat64(fileno(file.get()), &st);
#else
	struct stat st;
	int ret = fstat(fileno(file.get()), &st);
	if (ret && (errno == EOVERFLOW)) {
		// on 32-bit systems, the fstat() call returns a EOVERFLOW
		// error in case the file is bigger than (1<<31)-1 bytes
		throw FileException("Files >= 2GB are not supported on "
		                    "32-bit platforms: ", getURL());
	}
#endif
	if (ret) {
		throw FileException("Cannot get file size");
	}
	return st.st_size;
}

void LocalFile::seek(size_t pos)
{
#ifdef _WIN32
	int ret = _fseeki64(file.get(), pos, SEEK_SET);
#else
	int ret = fseek(file.get(), narrow_cast<long>(pos), SEEK_SET);
#endif
	if (ret != 0) {
		throw FileException("Error seeking file");
	}
}

size_t LocalFile::getPos()
{
	return ftell(file.get());
}

#if HAVE_FTRUNCATE
void LocalFile::truncate(size_t size)
{
	int fd = fileno(file.get());
	if (ftruncate(fd, narrow_cast<off_t>(size))) {
		throw FileException("Error truncating file");
	}
}
#endif

void LocalFile::flush()
{
	fflush(file.get());
}

const std::string& LocalFile::getURL() const
{
	return filename;
}

std::string LocalFile::getLocalReference()
{
	return filename;
}

bool LocalFile::isReadOnly() const
{
	return readOnly;
}

time_t LocalFile::getModificationDate()
{
#ifdef _WIN32
	// Normal fstat compiles but doesn't seem to be working the same
	// as on POSIX, regarding size support.
	struct _stat64 st;
	int ret = _fstat64(fileno(file.get()), &st);
#else
	struct stat st;
	int ret = fstat(fileno(file.get()), &st);
#endif
	if (ret) {
		throw FileException("Cannot stat file");
	}
	return st.st_mtime;
}

} // namespace openmsx
