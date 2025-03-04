#include "File.hh"

#include "Filename.hh"
#include "LocalFile.hh"
#include "GZFileAdapter.hh"
#include "ZipFileAdapter.hh"

#include "checked_cast.hh"
#include "ranges.hh"

#include <algorithm>
#include <array>
#include <memory>

namespace openmsx {

File::File() = default;

[[nodiscard]] static std::unique_ptr<FileBase> init(std::string filename, File::OpenMode mode)
{
	static constexpr std::array<uint8_t, 3> GZ_HEADER  = {0x1F, 0x8B, 0x08};
	static constexpr std::array<uint8_t, 4> ZIP_HEADER = {0x50, 0x4B, 0x03, 0x04};

	std::unique_ptr<FileBase> file = std::make_unique<LocalFile>(std::move(filename), mode);
	if (file->getSize() >= 4) {
		std::array<uint8_t, 4> buf;
		file->read(buf);
		file->seek(0);
		if (std::ranges::equal(subspan<3>(buf), GZ_HEADER)) {
			file = std::make_unique<GZFileAdapter>(std::move(file));
		} else if (std::ranges::equal(subspan<4>(buf), ZIP_HEADER)) {
			file = std::make_unique<ZipFileAdapter>(std::move(file));
		} else {
			// only pre-cache non-compressed files
			if (mode == File::OpenMode::PRE_CACHE) {
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

void File::read(std::span<uint8_t> buffer)
{
	file->read(buffer);
}

void File::write(std::span<const uint8_t> buffer)
{
	file->write(buffer);
}

std::span<const uint8_t> File::mmap()
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
	file->truncate(size);
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
