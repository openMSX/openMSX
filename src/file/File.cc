#include "File.hh"
#include "Filename.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"
#include "checked_cast.hh"
#include <cstring>
#include <memory>

namespace openmsx {

File::File() = default;

[[nodiscard]] static std::unique_ptr<FileBase> init(std::string filename, File::OpenMode mode)
{
	static constexpr uint8_t GZ_HEADER[3]  = { 0x1F, 0x8B, 0x08 };
	static constexpr uint8_t ZIP_HEADER[4] = { 0x50, 0x4B, 0x03, 0x04 };

	std::unique_ptr<FileBase> file = std::make_unique<LocalFile>(std::move(filename), mode);
	if (file->getSize() >= 4) {
		uint8_t buf[4];
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

File::File(std::string filename, OpenMode mode)
	: file(init(std::move(filename), mode))
{
}

File::File(const Filename& filename, OpenMode mode)
	: File(filename.getResolved(), mode)
{
}

File::File(Filename&& filename, OpenMode mode)
	: File(std::move(filename).getResolved(), mode)
{
}

File::File(std::string filename, const char* mode)
	: file(std::make_unique<LocalFile>(std::move(filename), mode))
{
}

File::File(const Filename& filename, const char* mode)
	: File(filename.getResolved(), mode)
{
}

File::File(Filename&& filename, const char* mode)
	: File(std::move(filename).getResolved(), mode)
{
}

File::File(File&& other) noexcept
	: file(std::move(other.file))
{
}

File::File(std::unique_ptr<FileBase> file_)
	: file(std::move(file_))
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

span<const uint8_t> File::mmap()
{
	return file->mmap();
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

const std::string& File::getURL() const
{
	return file->getURL();
}

std::string File::getLocalReference() const
{
	return file->getLocalReference();
}

std::string_view File::getOriginalName()
{
	std::string_view orig = file->getOriginalName();
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
