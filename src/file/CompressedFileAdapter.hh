#ifndef COMPRESSEDFILEADAPTER_HH
#define COMPRESSEDFILEADAPTER_HH

#include "FileBase.hh"

#include "MemBuffer.hh"
#include "zstring_view.hh"

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
	[[nodiscard]] MappedFileImpl mmap(size_t extra, bool is_const) final;
	[[nodiscard]] size_t getSize() final;
	void seek(size_t pos) final;
	[[nodiscard]] size_t getPos() final;
	void truncate(size_t size) final;
	void flush() final;
	[[nodiscard]] zstring_view getOriginalName() final;
	[[nodiscard]] bool isReadOnly() const final;
	[[nodiscard]] time_t getModificationDate() final;

protected:
	explicit CompressedFileAdapter(std::unique_ptr<FileBase> file, zstring_view filename);
	~CompressedFileAdapter() override;
	virtual void decompress(FileBase& file, Decompressed& decompressed) = 0;

private:
	void decompress();

private:
	// invariant: exactly one of 'file' and 'decompressed' is '!= nullptr'
	std::unique_ptr<FileBase> file;
	std::string filename;
	const Decompressed* decompressed = nullptr;
	size_t pos = 0;
};

} // namespace openmsx

#endif
