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
	LocalFile(string_view filename, File::OpenMode mode);
	LocalFile(string_view filename, const char* mode);
	~LocalFile();
	void read (void* buffer, size_t num) override;
	void write(const void* buffer, size_t num) override;
#if HAVE_MMAP || defined _WIN32
	const byte* mmap(size_t& size) override;
	void munmap() override;
#endif
	size_t getSize() override;
	void seek(size_t pos) override;
	size_t getPos() override;
#if HAVE_FTRUNCATE
	void truncate(size_t size) override;
#endif
	void flush() override;
	const std::string getURL() const override;
	const std::string getLocalReference() override;
	bool isReadOnly() const override;
	time_t getModificationDate() override;

	void preCacheFile();

private:
	std::string filename;
	FileOperations::FILE_t file;
#if HAVE_MMAP
	byte* mmem;
#endif
#if defined _WIN32
	byte* mmem;
	HANDLE hMmap;
#endif
	std::unique_ptr<PreCacheFile> cache;
	bool readOnly;
};

} // namespace openmsx

#endif
