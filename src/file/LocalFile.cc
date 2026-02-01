#include "LocalFile.hh"

#include "FileException.hh"
#include "FileNotFoundException.hh"
#include "MappedFile.hh"

#include "narrow.hh"
#include "one_of.hh"

#include "unistdp.hh"
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __unix__
#include <fcntl.h>
#endif
#if HAVE_MMAP
  #include <sys/mman.h>
#endif
#ifdef _WIN32
  #include <io.h>
  #include <iostream>
  #include <windows.h>
#endif

#include <cassert>
#include <cerrno>
#include <cstring> // for strchr, strerror

namespace openmsx {

LocalFile::LocalFile(zstring_view filename, File::OpenMode mode)
{
	auto name = FileOperations::getNativePath(filename);
	if (mode == File::OpenMode::TRUNCATE) {
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

LocalFile::LocalFile(zstring_view filename, const char* mode)
{
	assert(strchr(mode, 'b'));
	auto name = FileOperations::getNativePath(filename);
	file = FileOperations::openFile(name, mode);
	if (!file) {
		throw FileException("Error opening file \"", filename, '"');
	}
	(void)getSize(); // query filesize, but ignore result
}

void LocalFile::preCacheFile()
{
#ifdef __unix__
	// Prefetch first 1 MB (POSIX_FADV_WILLNEED is a non-blocking hint)
	off_t prefetch_size = 1024 * 1024;
	posix_fadvise(fileno(file.get()), 0, prefetch_size, POSIX_FADV_WILLNEED);
#endif
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

MappedFileImpl LocalFile::mmap(size_t extra, bool is_const)
{
	return {*this, extra, is_const};
}

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
		                    "32-bit platforms");
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

bool LocalFile::isLocalFile() const
{
	return true;
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
