// $Id$

#ifndef __COMPRESSEDFILEADAPTER_HH__
#define __COMPRESSEDFILEADAPTER_HH__

#include <memory>
#include "FileBase.hh"

namespace openmsx {

class CompressedFileAdapter : public FileBase
{
public:
	virtual void read(byte* buffer, unsigned num);
	virtual void write(const byte* buffer, unsigned num);
	virtual unsigned getSize();
	virtual void seek(unsigned pos);
	virtual unsigned getPos();
	virtual void truncate(unsigned size);
	virtual const std::string getURL() const;
	virtual const std::string getLocalName();
	virtual const std::string getOriginalName();
	virtual bool isReadOnly() const;
	virtual time_t getModificationDate();

protected:
	CompressedFileAdapter(std::auto_ptr<FileBase> file);
	virtual ~CompressedFileAdapter();
	virtual void decompress() = 0;

	const std::auto_ptr<FileBase> file;
	byte* buf;
	unsigned size;
	std::string originalName;

private:
	void fillBuffer();
	
	unsigned pos;
	static int tmpCount;	   // nb of files in tmp dir
	static std::string tmpDir; // name of tmp dir (when tmpCount > 0)
	char* localName;	   // name of tmp file (when != 0)
};

} // namespace openmsx

#endif

