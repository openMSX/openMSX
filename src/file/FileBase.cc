// $Id$

#include <cassert>
#include <algorithm>
#include "FileBase.hh"

using std::min;

namespace openmsx {

FileBase::FileBase() throw(FileException)
	: mmem(NULL)
{
}

FileBase::~FileBase()
{
	munmap();
}

byte* FileBase::mmap(bool writeBack) throw(FileException)
{
	if (!mmem) {
		mmapWrite = writeBack;
		mmapSize = getSize();
		mmem = new byte[mmapSize];
		read(mmem, mmapSize);
	}
	return mmem;
}

void FileBase::munmap() throw(FileException)
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

 void FileBase::truncate(unsigned size) throw(FileException)
{
	int grow = size - getSize();
	if (grow < 0) {
		PRT_DEBUG("Default truncate() can't shrink file!");
		return;
	}
	const int BUF_SIZE = 4096;
	byte buf[BUF_SIZE];
	memset(buf, 0, BUF_SIZE);
	while (grow > 0) {
		write(buf, min(BUF_SIZE, grow));
		grow -= BUF_SIZE;
	}
}

} // namespace openmsx
