// $Id$

#ifndef COMPRESSEDFILEADAPTER_HH
#define COMPRESSEDFILEADAPTER_HH

#include "FileBase.hh"
#include <memory>

namespace openmsx {

class CompressedFileAdapter : public FileBase
{
public:
	virtual void read(void* buffer, unsigned num);
	virtual void write(const void* buffer, unsigned num);
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
	virtual void decompress() = 0;

	const std::auto_ptr<FileBase> file;
	byte* buf;
	std::string originalName;
	unsigned size;

private:
	void fillBuffer();

	unsigned pos;
};

} // namespace openmsx

#endif
