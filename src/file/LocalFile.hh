// $Id$

#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include "FileBase.hh"
#include "File.hh"
#include "config.h"
#include <cstdio>


namespace openmsx {

class LocalFile : public FileBase
{
public:
	LocalFile(const string &filename, OpenMode mode) throw(FileException);
	virtual ~LocalFile();
	virtual void read (byte* buffer, unsigned num) throw(FileException);
	virtual void write(const byte* buffer, unsigned num) throw(FileException);
#ifdef	HAVE_MMAP
	virtual byte* mmap(bool writeBack) throw(FileException);
	virtual void munmap() throw();
#endif
	virtual unsigned getSize() throw();
	virtual void seek(unsigned pos) throw(FileException);
	virtual unsigned getPos() throw();
#ifdef HAVE_FTRUNCATE
	virtual void truncate(unsigned size) throw(FileException);
#endif
	virtual const string getURL() const throw();
	virtual const string getLocalName() throw();
	virtual bool isReadOnly() const throw();

private:
	string filename;
	FILE* file;
	bool readOnly;
};

} // namespace openmsx

#endif

