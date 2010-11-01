// $Id$

#include "File.hh"
#include "Filename.hh"
#include "FilePool.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"
#include "StringOp.hh"
#include "checked_cast.hh"
#include <cassert>
#include <cstring>

using std::string;

namespace openmsx {

static std::auto_ptr<FileBase> init(const string& url, File::OpenMode mode)
{
	static const byte GZ_HEADER[3]  = { 0x1F, 0x8B, 0x08 };
	static const byte ZIP_HEADER[4] = { 0x50, 0x4B, 0x03, 0x04 };

	std::auto_ptr<FileBase> file(new LocalFile(url, mode));
	byte buf[4];
	if (file->getSize() >= 4) {
		file->read(buf, 4);
		file->seek(0);
		if (memcmp(buf, GZ_HEADER, 3) == 0) {
			file.reset(new GZFileAdapter(file));
		} else if (memcmp(buf, ZIP_HEADER, 4) == 0) {
			file.reset(new ZipFileAdapter(file));
		} else {
			// only pre-cache non-compressed files
			if (mode == File::PRE_CACHE) {
				checked_cast<LocalFile*>(file.get())->preCacheFile();
			}
		}
	}
	return file;
}

File::File(const Filename& filename, OpenMode mode)
	: file(init(filename.getResolved(), mode))
	, filepool(NULL)
{
}

File::File(const string& url, OpenMode mode)
	: file(init(url, mode))
	, filepool(NULL)
{
}

File::File(const std::string& filename, const char* mode)
	: file(new LocalFile(filename, mode))
	, filepool(NULL)
{
}

File::File(const Filename& filename, const char* mode)
	: file(new LocalFile(filename.getResolved(), mode))
	, filepool(NULL)
{
}

File::~File()
{
}

void File::read(void* buffer, unsigned num)
{
	file->read(buffer, num);
}

void File::write(const void* buffer, unsigned num)
{
	if (filepool) {
		filepool->removeSha1Sum(*this);
	}
	file->write(buffer, num);
}

const byte* File::mmap()
{
	return file->mmap();
}

void File::munmap()
{
	file->munmap();
}

unsigned File::getSize()
{
	return file->getSize();
}

void File::seek(unsigned pos)
{
	file->seek(pos);
}

unsigned File::getPos()
{
	return file->getPos();
}

void File::truncate(unsigned size)
{
	return file->truncate(size);
}

void File::flush()
{
	file->flush();
}

const string File::getURL() const
{
	return file->getURL();
}

const string File::getLocalReference() const
{
	return file->getLocalReference();
}

const string File::getOriginalName()
{
	return file->getOriginalName();
}

bool File::isReadOnly() const
{
	return file->isReadOnly();
}

time_t File::getModificationDate()
{
	return file->getModificationDate();
}

string File::getSha1Sum()
{
	assert(filepool); // must be set
	return filepool->getSha1Sum(*this);
}

void File::setFilePool(FilePool& filepool_)
{
	assert(!filepool); // can only be set once
	filepool = &filepool_;
}

} // namespace openmsx
