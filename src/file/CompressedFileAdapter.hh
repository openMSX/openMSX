// $Id$

#ifndef COMPRESSEDFILEADAPTER_HH
#define COMPRESSEDFILEADAPTER_HH

#include "FileBase.hh"
#include "shared_ptr.hh"
#include <vector>
#include <memory>

namespace openmsx {

class CompressedFileAdapter : public FileBase
{
public:
	struct Decompressed {
		std::vector<byte> buf;
		std::string originalName;
		std::string cachedURL;
		time_t cachedModificationDate;
	};

	virtual void read(void* buffer, unsigned num);
	virtual void write(const void* buffer, unsigned num);
	virtual const byte* mmap();
	virtual void munmap();
	virtual unsigned getSize();
	virtual void seek(unsigned pos);
	virtual unsigned getPos();
	virtual void truncate(unsigned size);
	virtual void flush();
	virtual const std::string getURL() const;
	virtual const std::string getOriginalName();
	virtual bool isReadOnly() const;
	virtual time_t getModificationDate();

protected:
	explicit CompressedFileAdapter(std::auto_ptr<FileBase> file);
	virtual ~CompressedFileAdapter();
	virtual void decompress(FileBase& file, Decompressed& decompressed) = 0;

private:
	void decompress();

	std::auto_ptr<FileBase> file;
	shared_ptr<Decompressed> decompressed;
	unsigned pos;
};

} // namespace openmsx

#endif
