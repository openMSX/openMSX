#ifndef MEMORYBUFFERFILE_HH
#define MEMORYBUFFERFILE_HH

#include "FileBase.hh"

namespace openmsx {

class File;

class MemoryBufferFile final : public FileBase
{
public:
	MemoryBufferFile(span<const uint8_t> buffer_)
		: buffer(buffer_) {}

	void read(void* dst, size_t num) override;
	void write(const void* src, size_t num) override;

	size_t getSize() override;
	void seek(size_t newPos) override;
	size_t getPos() override;
	void flush() override;

	const std::string& getURL() const override;
	bool isReadOnly() const override;
	time_t getModificationDate() override;

private:
	span<const uint8_t> buffer;
	size_t pos = 0;
};

File memory_buffer_file(span<const uint8_t> buffer);

} // namespace openmsx

#endif
