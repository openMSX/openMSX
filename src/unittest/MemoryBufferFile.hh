#ifndef MEMORYBUFFERFILE_HH
#define MEMORYBUFFERFILE_HH

#include "FileBase.hh"

namespace openmsx {

class File;

class MemoryBufferFile final : public FileBase
{
public:
	explicit MemoryBufferFile(std::span<const uint8_t> buffer_)
		: buffer(buffer_) {}

	void read(std::span<uint8_t> dst) override;
	void write(std::span<const uint8_t> src) override;
	[[nodiscard]] MappedFileImpl mmap(size_t extra, bool is_const) override;

	[[nodiscard]] size_t getSize() override;
	void seek(size_t newPos) override;
	[[nodiscard]] size_t getPos() override;
	void flush() override;

	[[nodiscard]] bool isReadOnly() const override;
	[[nodiscard]] time_t getModificationDate() override;

private:
	std::span<const uint8_t> buffer;
	size_t pos = 0;
};

File memory_buffer_file(std::span<const uint8_t> buffer);

} // namespace openmsx

#endif
