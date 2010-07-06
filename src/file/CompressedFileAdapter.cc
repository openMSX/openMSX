// $Id$

#include "CompressedFileAdapter.hh"
#include "FileException.hh"
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <map>

using std::string;
using std::map;
using std::vector;

namespace openmsx {

typedef map<string, shared_ptr<CompressedFileAdapter::Decompressed> > DecompressCache;
static DecompressCache decompressCache;


CompressedFileAdapter::CompressedFileAdapter(std::auto_ptr<FileBase> file_)
	: file(file_), pos(0)
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
		decompressed.reset(new Decompressed());
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
	vector<byte>& buf = decompressed->buf;
	if (!buf.empty()) {
		memcpy(buffer, &buf[pos], num);
	} else {
		// in DEBUG mode buf[0] triggers an error on an empty vector
		//  vector::data() will solve this in new C++ standard
		assert((num == 0) && (pos == 0));
	}
	pos += num;
}

void CompressedFileAdapter::write(const void* /*buffer*/, unsigned /*num*/)
{
	throw FileException("Writing to compressed files not yet supported");
}

const byte* CompressedFileAdapter::mmap()
{
	decompress();
	vector<byte>& buf = decompressed->buf;
	if (!buf.empty()) { // see read()
		return &buf[0];
	} else {
		return NULL;
	}
}

void CompressedFileAdapter::munmap()
{
	// nothing
}

unsigned CompressedFileAdapter::getSize()
{
	decompress();
	return unsigned(decompressed->buf.size());
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
	return decompressed.get() ? decompressed->cachedURL
	                          : file->getURL();
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
	return decompressed.get() ? decompressed->cachedModificationDate
	                          : file->getModificationDate();
}

} // namespace openmsx
