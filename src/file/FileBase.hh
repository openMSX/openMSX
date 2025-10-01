#ifndef FILEBASE_HH
#define FILEBASE_HH

#include <cstdint>
#include <ctime>
#include <span>
#include <string>
#include <string_view>

namespace openmsx {

class MappedFileImpl;

class FileBase
{
public:
	virtual ~FileBase() = default;

	virtual void read(std::span<uint8_t> buffer) = 0;
	virtual void write(std::span<const uint8_t> buffer) = 0;
	[[nodiscard]] virtual MappedFileImpl mmap(size_t extra, bool is_const) = 0;
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
};

} // namespace openmsx

#endif
