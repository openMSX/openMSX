#include "File.hh"
#include "Filename.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"
#include "checked_cast.hh"
#include <cassert>
#include <cstring>
#include <memory>

using std::string;

namespace openmsx {

File::File() = default;

static std::unique_ptr<FileBase> init(string_view filename, File::OpenMode mode)
{
	static const byte GZ_HEADER[3]  = { 0x1F, 0x8B, 0x08 };
	static const byte ZIP_HEADER[4] = { 0x50, 0x4B, 0x03, 0x04 };

	std::unique_ptr<FileBase> file = std::make_unique<LocalFile>(filename, mode);
	if (file->getSize() >= 4) {
		byte buf[4];
		file->read(buf, 4);
		file->seek(0);
		if (memcmp(buf, GZ_HEADER, 3) == 0) {
			file = std::make_unique<GZFileAdapter>(std::move(file));
		} else if (memcmp(buf, ZIP_HEADER, 4) == 0) {
			file = std::make_unique<ZipFileAdapter>(std::move(file));
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
{
}

File::File(string_view filename, OpenMode mode)
	: file(init(filename, mode))
{
}

File::File(string_view filename, const char* mode)
	: file(std::make_unique<LocalFile>(filename, mode))
{
}

File::File(const Filename& filename, const char* mode)
	: file(std::make_unique<LocalFile>(filename.getResolved(), mode))
{
}

File::File(File&& other) noexcept
	: file(std::move(other.file))
{
}

File::~File() = default;

File& File::operator=(File&& other) noexcept
{
	file = std::move(other.file);
	return *this;
}

void File::close()
{
	file.reset();
}

void File::read(void* buffer, size_t num)
{
	file->read(buffer, num);
}

void File::write(const void* buffer, size_t num)
{
	file->write(buffer, num);
}

const byte* File::mmap(size_t& size)
{
	return file->mmap(size);
}

void File::munmap()
{
	file->munmap();
}

size_t File::getSize()
{
	return file->getSize();
}

void File::seek(size_t pos)
{
	file->seek(pos);
}

size_t File::getPos()
{
	return file->getPos();
}

void File::truncate(size_t size)
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
	string orig = file->getOriginalName();
	return !orig.empty() ? orig : getURL();
}

bool File::isReadOnly() const
{
	return file->isReadOnly();
}

time_t File::getModificationDate()
{
	return file->getModificationDate();
}

} // namespace openmsx
