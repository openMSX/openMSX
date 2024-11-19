#include "FileBase.hh"
#include "FileOperations.hh"
#include "ranges.hh"
#include <algorithm>
#include <array>

namespace openmsx {

std::span<const uint8_t> FileBase::mmap()
{
	auto size = getSize();
	if (mmapBuf.empty()) {
		auto pos = getPos();
		seek(0);

		MemBuffer<uint8_t> tmpBuf(size);
		read(std::span{tmpBuf});
		std::swap(mmapBuf, tmpBuf);

		seek(pos);
	}
	return std::span{mmapBuf};
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

	std::array<uint8_t, 4096> buf = {}; // zero-initialized
	while (remaining) {
		auto chunkSize = std::min(buf.size(), remaining);
		write(subspan(buf, 0, chunkSize));
		remaining -= chunkSize;
	}
}

std::string FileBase::getLocalReference()
{
	// default implementation, file is not backed (uncompressed) on
	// the local file system
	return {};
}

std::string_view FileBase::getOriginalName()
{
	// default implementation just returns filename portion of URL
	return FileOperations::getFilename(getURL());
}

} // namespace openmsx
