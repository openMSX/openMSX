#include "MemoryBufferFile.hh"
#include "File.hh"
#include "FileException.hh"
#include "ranges.hh"
#include <memory>

namespace openmsx {

void MemoryBufferFile::read(std::span<uint8_t> dst)
{
	if (getSize() < (getPos() + dst.size())) {
		throw FileException("Read beyond end of file");
	}
	copy_to_range(buffer.subspan(pos, dst.size()), dst);
	pos += dst.size();
}

void MemoryBufferFile::write(std::span<const uint8_t> /*src*/)
{
	throw FileException("Writing to MemoryBufferFile not supported");
}

MappedFileImpl MemoryBufferFile::mmap(size_t extra, bool is_const)
{
	return {buffer, extra, is_const};
}

size_t MemoryBufferFile::getSize()
{
	return buffer.size();
}

void MemoryBufferFile::seek(size_t newPos)
{
	pos = newPos;
}

size_t MemoryBufferFile::getPos()
{
	return pos;
}

void MemoryBufferFile::flush()
{
	// nothing
}

const std::string& MemoryBufferFile::getURL() const
{
	static const std::string EMPTY;
	return EMPTY;
}

bool MemoryBufferFile::isReadOnly() const
{
	return true;
}

time_t MemoryBufferFile::getModificationDate()
{
	return 0;
}


File memory_buffer_file(std::span<const uint8_t> buffer)
{
	return File(std::make_unique<MemoryBufferFile>(buffer));
}

} // namespace openmsx
