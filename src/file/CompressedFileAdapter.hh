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
		MemBuffer<byte> buf;
		std::string originalName;
		std::string cachedURL;
		time_t cachedModificationDate;
	};

	virtual void read(void* buffer, size_t num) final;
	virtual void write(const void* buffer, size_t num) final;
	virtual const byte* mmap(size_t& size) final;
	virtual void munmap() final;
	virtual size_t getSize() final;
	virtual void seek(size_t pos) final;
	virtual size_t getPos() final;
	virtual void truncate(size_t size) final;
	virtual void flush() final;
	virtual const std::string getURL() const final;
	virtual const std::string getOriginalName() final;
	virtual bool isReadOnly() const final;
	virtual time_t getModificationDate() final;

protected:
	explicit CompressedFileAdapter(std::unique_ptr<FileBase> file);
	~CompressedFileAdapter();
	virtual void decompress(FileBase& file, Decompressed& decompressed) = 0;

private:
	void decompress();

	std::unique_ptr<FileBase> file;
	std::shared_ptr<Decompressed> decompressed;
	size_t pos;
};

} // namespace openmsx

#endif
