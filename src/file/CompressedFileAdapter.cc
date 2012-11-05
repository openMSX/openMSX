// $Id$

#include "CompressedFileAdapter.hh"
#include "FileException.hh"
#include "StringMap.hh"
#include <cstdlib>
#include <cstring>
#include <cassert>

using std::string;

namespace openmsx {

typedef StringMap<std::shared_ptr<CompressedFileAdapter::Decompressed>> DecompressCache;
static DecompressCache decompressCache;


CompressedFileAdapter::CompressedFileAdapter(std::unique_ptr<FileBase> file_)
	: file(std::move(file_)), pos(0)
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
	DecompressCache::iterator it = decompressCache.find(getURL());
	decompressed.reset();
	if (it != decompressCache.end() && it->second.unique()) {
		// delete last user of Decompressed, remove from cache
		decompressCache.erase(it);
	}
}

void CompressedFileAdapter::decompress()
{
	if (decompressed.get()) return;

	string url = getURL();
	DecompressCache::iterator it = decompressCache.find(url);
	if (it != decompressCache.end()) {
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

void CompressedFileAdapter::read(void* buffer, unsigned num)
{
	decompress();
	const MemBuffer<byte>& buf = decompressed->buf;
	if (buf.size() < (pos + num)) {
		throw FileException("Read beyond end of file");
	}
	memcpy(buffer, buf.data() + pos, num);
	pos += num;
}

void CompressedFileAdapter::write(const void* /*buffer*/, unsigned /*num*/)
{
	throw FileException("Writing to compressed files not yet supported");
}

const byte* CompressedFileAdapter::mmap(unsigned& size)
{
	decompress();
	size = decompressed->buf.size();
	return reinterpret_cast<const byte*>(decompressed->buf.data());
}

void CompressedFileAdapter::munmap()
{
	// nothing
}

unsigned CompressedFileAdapter::getSize()
{
	decompress();
	return decompressed->buf.size();
}

void CompressedFileAdapter::seek(unsigned newpos)
{
	pos = newpos;
}

unsigned CompressedFileAdapter::getPos()
{
	return pos;
}

void CompressedFileAdapter::truncate(unsigned /*size*/)
{
	throw FileException("Truncating compressed files not yet supported.");
}

void CompressedFileAdapter::flush()
{
	// nothing because writing is not supported
}

const string CompressedFileAdapter::getURL() const
{
	return file.get() ? file->getURL()
	                  : decompressed->cachedURL;
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
	return file.get() ? file->getModificationDate()
	                  : decompressed->cachedModificationDate;
}

} // namespace openmsx
