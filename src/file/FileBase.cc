#include "FileBase.hh"
#include "FileOperations.hh"
#include <algorithm>
#include <cstring>

using std::string;

namespace openmsx {

const byte* FileBase::mmap(size_t& size)
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

void FileBase::truncate(size_t newSize)
{
	auto oldSize = getSize();
	if (newSize < oldSize) {
		// default truncate() can't shrink file
		return;
	}
	auto remaining = newSize - oldSize;
	seek(oldSize);

	static const size_t BUF_SIZE = 4096;
	byte buf[BUF_SIZE];
	memset(buf, 0, sizeof(buf));
	while (remaining) {
		auto chunkSize = std::min(BUF_SIZE, remaining);
		write(buf, chunkSize);
		remaining -= chunkSize;
	}
}

const string FileBase::getLocalReference()
{
	// default implementation, file is not backed (uncompressed) on
	// the local file system
	return {};
}

const string FileBase::getOriginalName()
{
	// default implementation just returns filename portion of URL
	return FileOperations::getFilename(getURL()).str();
}

} // namespace openmsx
