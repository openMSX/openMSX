#include "LocalFileReference.hh"

#include "File.hh"
#include "FileException.hh"
#include "FileOperations.hh"

#include "build-info.hh"

#include <cassert>
#include <cstdio>

namespace openmsx {

LocalFileReference::LocalFileReference(std::string filename_)
	: filename(std::move(filename_))
{
	File file(filename);
	if (file.isLocalFile()) {
		// file is backed on the (local) filesystem,
		// we can simply use the path to that file
		assert(tmpDir.empty()); // no need to delete file/dir later
		return;
	}

	// create temp dir
#if defined(_WIN32) || PLATFORM_ANDROID
	tmpDir = FileOperations::getTempDir() + FileOperations::nativePathSeparator + "openmsx";
#else
	// TODO - why not just use getTempDir()?
	tmpDir = strCat("/tmp/openmsx.", int(getpid()));
#endif
	// it's possible this directory already exists, in that case the
	// following function does nothing
	FileOperations::mkdirp(tmpDir);

	// create temp file
	auto fp = FileOperations::openUniqueFile(tmpDir, filename);
	if (!fp) {
		throw FileException("Couldn't create temp file");
	}

	// write temp file
	auto mmap = file.mmap<const char>();
	if (fwrite(mmap.data(), 1, mmap.size(), fp.get()) != mmap.size()) {
		throw FileException("Couldn't write temp file");
	}
}

LocalFileReference::LocalFileReference(LocalFileReference&& other) noexcept
	: filename(std::move(other.filename))
	, tmpDir  (std::move(other.tmpDir))
{
	other.tmpDir.clear();
}

LocalFileReference& LocalFileReference::operator=(LocalFileReference&& other) noexcept
{
	cleanup();
	filename = std::move(other.filename);
	tmpDir   = std::move(other.tmpDir);
	other.tmpDir.clear();
	return *this;
}

LocalFileReference::~LocalFileReference()
{
	cleanup();
}

void LocalFileReference::cleanup() const
{
	if (!tmpDir.empty()) {
		FileOperations::unlink(filename);
		// it's possible the directory is not empty, in that case
		// the following function will fail, we ignore that error
		FileOperations::rmdir(tmpDir);
	}
}

} // namespace openmsx
