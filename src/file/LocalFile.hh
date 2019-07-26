#ifndef LOCALFILE_HH
#define LOCALFILE_HH

#if defined _WIN32
#include <windows.h>
#endif
#include "File.hh"
#include "FileBase.hh"
#include "FileOperations.hh"
#include "systemfuncs.hh"
#include <cstdio>
#include <memory>

namespace openmsx {

class PreCacheFile;

class LocalFile final : public FileBase
{
public:
	LocalFile(std::string_view filename, File::OpenMode mode);
	LocalFile(std::string_view filename, const char* mode);
	~LocalFile() override;
	void read (void* buffer, size_t num) override;
	void write(const void* buffer, size_t num) override;
#if HAVE_MMAP || defined _WIN32
	span<uint8_t> mmap() override;
	void munmap() override;
#endif
	size_t getSize() override;
	void seek(size_t pos) override;
	size_t getPos() override;
#if HAVE_FTRUNCATE
	void truncate(size_t size) override;
#endif
	void flush() override;
	std::string getURL() const override;
	std::string getLocalReference() override;
	bool isReadOnly() const override;
	time_t getModificationDate() override;

	void preCacheFile();

private:
	std::string filename;
	FileOperations::FILE_t file;
#if HAVE_MMAP
	uint8_t* mmem;
#endif
#if defined _WIN32
	uint8_t* mmem;
	HANDLE hMmap;
#endif
	std::unique_ptr<PreCacheFile> cache;
	bool readOnly;
};

} // namespace openmsx

#endif
