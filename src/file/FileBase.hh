#ifndef FILEBASE_HH
#define FILEBASE_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class FileBase
{
public:
	virtual ~FileBase() = default;

	virtual void read(void* buffer, size_t num) = 0;
	virtual void write(const void* buffer, size_t num) = 0;

	// If you override mmap(), make sure to call munmap() in
	// your destructor.
	virtual const byte* mmap(size_t& size);
	virtual void munmap();

	virtual size_t getSize() = 0;
	virtual void seek(size_t pos) = 0;
	virtual size_t getPos() = 0;
	virtual void truncate(size_t size);
	virtual void flush() = 0;

	virtual const std::string getURL() const = 0;
	virtual const std::string getLocalReference();
	virtual const std::string getOriginalName();
	virtual bool isReadOnly() const = 0;
	virtual time_t getModificationDate() = 0;

private:
	MemBuffer<byte> mmapBuf;
};

} // namespace openmsx

#endif
