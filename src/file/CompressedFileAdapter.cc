#include "CompressedFileAdapter.hh"
#include "FileException.hh"
#include "StringMap.hh"
#include <cstring>

using std::string;

namespace openmsx {

static StringMap<std::shared_ptr<CompressedFileAdapter::Decompressed>> decompressCache;


CompressedFileAdapter::CompressedFileAdapter(std::unique_ptr<FileBase> file_)
	: file(std::move(file_)), pos(0)
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
	auto it = decompressCache.find(getURL());
	decompressed.reset();
	if (it != end(decompressCache) && it->second.unique()) {
		// delete last user of Decompressed, remove from cache
		decompressCache.erase(it);
	}
}

void CompressedFileAdapter::decompress()
{
	if (decompressed) return;

	string url = getURL();
	auto it = decompressCache.find(url);
	if (it != end(decompressCache)) {
		decompressed = it->second;
	} else {
		decompressed = std::make_shared<Decompressed>();
		decompress(*file, *decompressed);
		decompressed->cachedModificationDate = getModificationDate();
		decompressed->cachedURL = url;
		decompressCache[url] = decompressed;
	}

	// close original file after succesful decompress
	file.reset();
}

void CompressedFileAdapter::read(void* buffer, size_t num)
{
	decompress();
	const MemBuffer<byte>& buf = decompressed->buf;
	if (buf.size() < (pos + num)) {
		throw FileException("Read beyond end of file");
	}
	memcpy(buffer, buf.data() + pos, num);
	pos += num;
}

void CompressedFileAdapter::write(const void* /*buffer*/, size_t /*num*/)
{
	throw FileException("Writing to compressed files not yet supported");
}

const byte* CompressedFileAdapter::mmap(size_t& size)
{
	decompress();
	size = decompressed->buf.size();
	return reinterpret_cast<const byte*>(decompressed->buf.data());
}

void CompressedFileAdapter::munmap()
{
	// nothing
}

size_t CompressedFileAdapter::getSize()
{
	decompress();
	return decompressed->buf.size();
}

void CompressedFileAdapter::seek(size_t newpos)
{
	pos = newpos;
}

size_t CompressedFileAdapter::getPos()
{
	return pos;
}

void CompressedFileAdapter::truncate(size_t /*size*/)
{
	throw FileException("Truncating compressed files not yet supported.");
}

void CompressedFileAdapter::flush()
{
	// nothing because writing is not supported
}

const string CompressedFileAdapter::getURL() const
{
	return file ? file->getURL() : decompressed->cachedURL;
}

const string CompressedFileAdapter::getOriginalName()
{
	decompress();
	return decompressed->originalName;
}

bool CompressedFileAdapter::isReadOnly() const
{
	return true;
}

time_t CompressedFileAdapter::getModificationDate()
{
	return file ? file->getModificationDate()
	            : decompressed->cachedModificationDate;
}

} // namespace openmsx
