#ifndef MEMORYBUFFERFILE_HH
#define MEMORYBUFFERFILE_HH

#include "FileBase.hh"

namespace openmsx {

class File;

class MemoryBufferFile final : public FileBase
{
public:
	MemoryBufferFile(std::span<const uint8_t> buffer_)
		: buffer(buffer_) {}

	void read(std::span<uint8_t> dst) override;
	void write(std::span<const uint8_t> src) override;

	size_t getSize() override;
	void seek(size_t newPos) override;
	size_t getPos() override;
	void flush() override;

	const std::string& getURL() const override;
	bool isReadOnly() const override;
	time_t getModificationDate() override;

private:
	std::span<const uint8_t> buffer;
	size_t pos = 0;
};

File memory_buffer_file(std::span<const uint8_t> buffer);

} // namespace openmsx

#endif
