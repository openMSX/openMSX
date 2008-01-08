// $Id$

#include "CompressedFileAdapter.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include <cstdlib>
#include <cstring>

using std::string;

namespace openmsx {

CompressedFileAdapter::CompressedFileAdapter(std::auto_ptr<FileBase> file_)
	: file(file_), buf(0), pos(0)
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
	if (buf) {
		free(buf);
	}
}

void CompressedFileAdapter::fillBuffer()
{
	if (!buf) {
		decompress();
	}
}

void CompressedFileAdapter::read(void* buffer, unsigned num)
{
	fillBuffer();
	memcpy(buffer, &buf[pos], num);
	pos += num;
}

void CompressedFileAdapter::write(const void* /*buffer*/, unsigned /*num*/)
{
	throw FileException("Writing to compressed files not yet supported");
}

unsigned CompressedFileAdapter::getSize()
{
	fillBuffer();
	return size;
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
	return file->getURL();
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
	return file->getModificationDate();
}

} // namespace openmsx
