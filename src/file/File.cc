// $Id$

#include "File.hh"
#include "Filename.hh"
#include "FilePool.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"
#include "StringOp.hh"
#include <cassert>

using std::string;

namespace openmsx {

static File::OpenMode dontPreCache(File::OpenMode mode)
{
	return (mode == File::PRE_CACHE) ? File::NORMAL : mode;
}

static std::auto_ptr<FileBase> init(const string& url, File::OpenMode mode)
{
	std::auto_ptr<FileBase> result;
	if ((StringOp::endsWith(url, ".gz")) ||
	    (StringOp::endsWith(url, ".GZ"))) {
		std::auto_ptr<FileBase> tmp(new LocalFile(url, dontPreCache(mode)));
		result.reset(new GZFileAdapter(tmp));
	} else if ((StringOp::endsWith(url, ".zip")) ||
	           (StringOp::endsWith(url, ".ZIP"))) {
		std::auto_ptr<FileBase> tmp(new LocalFile(url, dontPreCache(mode)));
		result.reset(new ZipFileAdapter(tmp));
	} else {
		result.reset(new LocalFile(url, mode));
	}
	return result;
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
