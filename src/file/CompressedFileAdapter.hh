// $Id$

#ifndef __COMPRESSEDFILEADAPTER_HH__
#define __COMPRESSEDFILEADAPTER_HH__

#include "FileBase.hh"

namespace openmsx {

class CompressedFileAdapter : public FileBase
{
public:
	virtual void read(byte* buffer, unsigned num) throw();
	virtual void write(const byte* buffer, unsigned num) throw(FileException);
	virtual unsigned getSize() throw();
	virtual void seek(unsigned pos) throw();
	virtual unsigned getPos() throw();
	virtual void truncate(unsigned size) throw(FileException);
	virtual const string getURL() const throw(FileException);
	virtual const string getLocalName() throw(FileException);
	virtual bool isReadOnly() const throw();

protected:
	CompressedFileAdapter(FileBase* file) throw();
	virtual ~CompressedFileAdapter();

	FileBase* file;
	byte* buf;
	unsigned size;

private:
	unsigned pos;

	static int tmpCount;	// nb of files in tmp dir
	static string tmpDir;	// name of tmp dir (when tmpCount > 0)
	char* localName;	// name of tmp file (when != 0)
};

} // namespace openmsx

#endif

