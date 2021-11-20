#include "CompressedFileAdapter.hh"
#include "FileException.hh"
#include "hash_set.hh"
#include "xxhash.hh"
#include <cstring>

namespace openmsx {

struct GetURLFromDecompressed {
	template<typename Ptr> [[nodiscard]] const std::string& operator()(const Ptr& p) const {
		return p->cachedURL;
	}
};
static hash_set<std::unique_ptr<CompressedFileAdapter::Decompressed>,
                GetURLFromDecompressed, XXHasher> decompressCache;


CompressedFileAdapter::CompressedFileAdapter(std::unique_ptr<FileBase> file_)
	: file(std::move(file_))
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
	if (decompressed) {
		auto it = decompressCache.find(getURL());
		assert(it != end(decompressCache));
		assert(it->get() == decompressed);
		--(*it)->useCount;
		if ((*it)->useCount == 0) {
			// delete last user of Decompressed, remove from cache
			decompressCache.erase(it);
		}
	}
}

void CompressedFileAdapter::decompress()
{
	if (decompressed) return;

	const std::string& url = getURL();
	auto it = decompressCache.find(url);
	if (it == end(decompressCache)) {
		auto d = std::make_unique<Decompressed>();
		decompress(*file, *d);
		d->cachedModificationDate = getModificationDate();
		d->cachedURL = url;
		it = decompressCache.insert_noDuplicateCheck(std::move(d));
	}
	++(*it)->useCount;
	decompressed = it->get();

	// close original file after successful decompress
	file.reset();
}

void CompressedFileAdapter::read(void* buffer, size_t num)
{
	decompress();
	if (decompressed->size < (pos + num)) {
		throw FileException("Read beyond end of file");
	}
	const auto& buf = decompressed->buf;
	memcpy(buffer, buf.data() + pos, num);
	pos += num;
}

void CompressedFileAdapter::write(const void* /*buffer*/, size_t /*num*/)
{
	throw FileException("Writing to compressed files not yet supported");
}

span<const uint8_t> CompressedFileAdapter::mmap()
{
	decompress();
	return { decompressed->buf.data(), decompressed->size };
}

void CompressedFileAdapter::munmap()
{
	// nothing
}

size_t CompressedFileAdapter::getSize()
{
	decompress();
	return decompressed->size;
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

const std::string& CompressedFileAdapter::getURL() const
{
	return file ? file->getURL() : decompressed->cachedURL;
}

std::string_view CompressedFileAdapter::getOriginalName()
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
