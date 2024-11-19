#ifndef COMPRESSEDFILEADAPTER_HH
#define COMPRESSEDFILEADAPTER_HH

#include "FileBase.hh"
#include "MemBuffer.hh"
#include <memory>

namespace openmsx {

class CompressedFileAdapter : public FileBase
{
public:
	struct Decompressed {
		MemBuffer<uint8_t> buf;
		std::string originalName;
		std::string cachedURL;
		time_t cachedModificationDate;
		unsigned useCount = 0;
	};

	void read(std::span<uint8_t> buffer) final;
	void write(std::span<const uint8_t> buffer) final;
	[[nodiscard]] std::span<const uint8_t> mmap() final;
	void munmap() final;
	[[nodiscard]] size_t getSize() final;
	void seek(size_t pos) final;
	[[nodiscard]] size_t getPos() final;
	void truncate(size_t size) final;
	void flush() final;
	[[nodiscard]] const std::string& getURL() const final;
	[[nodiscard]] std::string_view getOriginalName() final;
	[[nodiscard]] bool isReadOnly() const final;
	[[nodiscard]] time_t getModificationDate() final;

protected:
	explicit CompressedFileAdapter(std::unique_ptr<FileBase> file);
	~CompressedFileAdapter() override;
	virtual void decompress(FileBase& file, Decompressed& decompressed) = 0;

private:
	void decompress();

private:
	// invariant: exactly one of 'file' and 'decompressed' is '!= nullptr'
	std::unique_ptr<FileBase> file;
	const Decompressed* decompressed = nullptr;
	size_t pos = 0;
};

} // namespace openmsx

#endif
