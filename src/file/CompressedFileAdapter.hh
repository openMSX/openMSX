// $Id$

#ifndef __COMPRESSEDFILEADAPTER_HH__
#define __COMPRESSEDFILEADAPTER_HH__

#include <memory>
#include "FileBase.hh"

using std::auto_ptr;

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
	virtual const string getURL() const;
	virtual const string getLocalName();
	virtual bool isReadOnly() const;
	virtual time_t getModificationDate();

protected:
	CompressedFileAdapter(auto_ptr<FileBase> file);
	virtual ~CompressedFileAdapter();
	virtual void decompress() = 0;

	const auto_ptr<FileBase> file;
	byte* buf;
	unsigned size;

private:
	void fillBuffer();
	
	unsigned pos;
	static int tmpCount;	// nb of files in tmp dir
	static string tmpDir;	// name of tmp dir (when tmpCount > 0)
	char* localName;	// name of tmp file (when != 0)
};

} // namespace openmsx

#endif

