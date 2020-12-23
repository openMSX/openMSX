#include "MemoryBufferFile.hh"
#include "File.hh"
#include "FileException.hh"
#include <cstring>
#include <memory>

namespace openmsx {

void MemoryBufferFile::read(void* dst, size_t num)
{
	if (getSize() < (getPos() + num)) {
		throw FileException("Read beyond end of file");
	}
	memcpy(dst, buffer.data() + pos, num);
	pos += num;
}

void MemoryBufferFile::write(const void* /*src*/, size_t /*num*/)
{
	throw FileException("Writing to MemoryBufferFile not supported");
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


File memory_buffer_file(span<const uint8_t> buffer)
{
	return File(std::make_unique<MemoryBufferFile>(buffer));
}

} // namespace openmsx
