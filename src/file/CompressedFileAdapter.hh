// $Id$

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

	virtual void read(void* buffer, size_t num);
	virtual void write(const void* buffer, size_t num);
	virtual const byte* mmap(size_t& size);
	virtual void munmap();
	virtual size_t getSize();
	virtual void seek(size_t pos);
	virtual size_t getPos();
	virtual void truncate(size_t size);
	virtual void flush();
	virtual const std::string getURL() const;
	virtual const std::string getOriginalName();
	virtual bool isReadOnly() const;
	virtual time_t getModificationDate();

protected:
	explicit CompressedFileAdapter(std::unique_ptr<FileBase> file);
	virtual ~CompressedFileAdapter();
	virtual void decompress(FileBase& file, Decompressed& decompressed) = 0;

private:
	void decompress();

	std::unique_ptr<FileBase> file;
	std::shared_ptr<Decompressed> decompressed;
	size_t pos;
};

} // namespace openmsx

#endif
