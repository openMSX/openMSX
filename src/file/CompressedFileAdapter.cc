// $Id$

#include "CompressedFileAdapter.hh"
#include "FileException.hh"
#include <cstdlib>
#include <cstring>
#include <cassert>

using std::string;

namespace openmsx {

CompressedFileAdapter::CompressedFileAdapter(std::auto_ptr<FileBase> file_)
	: file(file_), pos(0)
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
}

void CompressedFileAdapter::fillBuffer()
{
	if (file.get()) {
		decompress(*file);
		cachedModificationDate = getModificationDate();
		cachedURL = getURL();
		// close original file after succesful decompress
		file.reset();
	}
}

void CompressedFileAdapter::read(void* buffer, unsigned num)
{
	fillBuffer();
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

unsigned CompressedFileAdapter::getSize()
{
	fillBuffer();
	return unsigned(buf.size());
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
	                  : cachedURL;
}

const string CompressedFileAdapter::getOriginalName()
{
	fillBuffer();
	return originalName;
}

bool CompressedFileAdapter::isReadOnly() const
{
	return true;
}

time_t CompressedFileAdapter::getModificationDate()
{
	return file.get() ? file->getModificationDate()
	                  : cachedModificationDate;
}

} // namespace openmsx
