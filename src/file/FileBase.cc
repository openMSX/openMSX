#include "FileBase.hh"

#include "FileOperations.hh"

#include "ranges.hh"

#include <algorithm>
#include <array>

namespace openmsx {

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

bool FileBase::isLocalFile() const
{
	return false;
}

zstring_view FileBase::getOriginalName()
{
	// default implementation
	return {};
}

} // namespace openmsx
