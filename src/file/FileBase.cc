// $Id$

#include "FileBase.hh"
#include <cassert>


namespace openmsx {

FileBase::FileBase()
	: mmem(NULL)
{
}

FileBase::~FileBase()
{
	munmap();
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

} // namespace openmsx
