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

class LocalFile : public FileBase
{
public:
	LocalFile(string_ref filename, File::OpenMode mode);
	LocalFile(string_ref filename, const char* mode);
	virtual ~LocalFile();
	virtual void read (void* buffer, size_t num);
	virtual void write(const void* buffer, size_t num);
#if HAVE_MMAP || defined _WIN32
	virtual const byte* mmap(size_t& size);
	virtual void munmap();
#endif
	virtual size_t getSize();
	virtual void seek(size_t pos);
	virtual size_t getPos();
#if HAVE_FTRUNCATE
	virtual void truncate(size_t size);
#endif
	virtual void flush();
	virtual const std::string getURL() const;
	virtual const std::string getLocalReference();
	virtual bool isReadOnly() const;
	virtual time_t getModificationDate();

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
