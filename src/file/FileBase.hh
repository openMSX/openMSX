// $Id$

#ifndef __FILEBASE_HH__
#define __FILEBASE_HH__

#include <string>
#include "openmsx.hh"
#include "File.hh"

using std::string;

namespace openmsx {

class FileBase
{
public:
	FileBase() throw(FileException);
	virtual ~FileBase();
	
	virtual void read (byte* buffer, unsigned num) throw(FileException) = 0;
	virtual void write(const byte* buffer, unsigned num) throw(FileException) = 0;
	
	// If you override mmap(), make sure to call munmap() in
	// your destructor.
	virtual byte* mmap(bool writeBack = false) throw(FileException);
	virtual void munmap() throw(FileException);
	
	virtual unsigned getSize() throw(FileException) = 0;
	virtual void seek(unsigned pos) throw(FileException) = 0;
	virtual unsigned getPos() throw(FileException) = 0;
	virtual void truncate(unsigned size) throw(FileException);
	
	virtual const string getURL() const throw(FileException) = 0;
	virtual const string getLocalName() throw(FileException) = 0;
	virtual bool isReadOnly() const throw(FileException) = 0;

protected:
	byte* mmem;

private:
	bool mmapWrite;
	unsigned mmapSize;
};

} // namespace openmsx

#endif
