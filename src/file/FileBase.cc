// $Id$

#include "FileBase.hh"
#include "FileOperations.hh"
#include <algorithm>
#include <cstring>

using std::string;

namespace openmsx {

FileBase::FileBase()
{
}

FileBase::~FileBase()
{
	munmap();
}

const byte* FileBase::mmap(unsigned& size)
{
	if (mmapBuf.empty()) {
		size = getSize();
		MemBuffer<byte> tmpBuf(size);
		read(tmpBuf.data(), size);
		std::swap(mmapBuf, tmpBuf);
	}
	return mmapBuf.data();
}

void FileBase::munmap()
{
	mmapBuf.clear();
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
	memset(buf, 0, sizeof(buf));
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
	return FileOperations::getFilename(getURL()).str();
}

} // namespace openmsx
