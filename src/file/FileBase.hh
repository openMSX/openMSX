#ifndef FILEBASE_HH
#define FILEBASE_HH

#include "MemBuffer.hh"

#include <cstdint>
#include <ctime>
#include <span>
#include <string>

namespace openmsx {

class FileBase
{
public:
	virtual ~FileBase() = default;

	virtual void read(std::span<uint8_t> buffer) = 0;
	virtual void write(std::span<const uint8_t> buffer) = 0;

	// If you override mmap(), make sure to call munmap() in
	// your destructor.
	[[nodiscard]] virtual std::span<const uint8_t> mmap();
	virtual void munmap();

	[[nodiscard]] virtual size_t getSize() = 0;
	virtual void seek(size_t pos) = 0;
	[[nodiscard]] virtual size_t getPos() = 0;
	virtual void truncate(size_t size);
	virtual void flush() = 0;

	[[nodiscard]] virtual const std::string& getURL() const = 0;
	[[nodiscard]] virtual std::string getLocalReference();
	[[nodiscard]] virtual std::string_view getOriginalName();
	[[nodiscard]] virtual bool isReadOnly() const = 0;
	[[nodiscard]] virtual time_t getModificationDate() = 0;

private:
	MemBuffer<uint8_t> mmapBuf;
};

} // namespace openmsx

#endif
