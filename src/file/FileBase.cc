// $Id$

#include "FileBase.hh"
#include <cassert>

FileBase::~FileBase()
{
	// Make sure the user first munmapped before destroying
	// this object. We can't do it automatically because we
	// can't use virtual functions in destructor.
	assert(!mmem);
}

byte* FileBase::mmap(bool writeBack)
{
	if (!mmem) {
		mmapWrite = writeBack;
		mmapSize = getSize();
		mmem = new byte[mmapSize];
		read(mmem, mmapSize);
	}
	return mmem;
}

void FileBase::munmap()
{
	if (mmem) {
		if (mmapWrite) {
			seek(0);
			write(mmem, mmapSize);
		}
		delete[] mmem;
		mmem = NULL;
	}
}
