// $Id$

#include "FileBase.hh"
#include "FileOperations.hh"
#include <algorithm>
#include <cstring>

using std::string;

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

void FileBase::truncate(unsigned newSize)
{
	unsigned oldSize = getSize();
	if (newSize < oldSize) {
		PRT_DEBUG("Default truncate() can't shrink file!");
		return;
	}
	unsigned remaining = newSize - oldSize;
	seek(oldSize);

	const unsigned BUF_SIZE = 4096;
	byte buf[BUF_SIZE];
	memset(buf, 0, BUF_SIZE);
	while (remaining) {
		unsigned chunkSize = std::min(BUF_SIZE, remaining);
		write(buf, chunkSize);
		remaining -= chunkSize;
	}
}

const string FileBase::getLocalReference()
{
	// default implementation, file is not backed (uncompressed) on
	// the local file system
	return "";
}

const string FileBase::getOriginalName()
{
	// default implementation just returns filename portion of URL
	return FileOperations::getFilename(getURL());
}

} // namespace openmsx
