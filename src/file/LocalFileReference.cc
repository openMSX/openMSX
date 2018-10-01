#include "LocalFileReference.hh"
#include "File.hh"
#include "Filename.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "build-info.hh"
#include <cstdio>
#include <cassert>

using std::string;

namespace openmsx {

LocalFileReference::LocalFileReference(const Filename& filename)
{
	init(filename.getResolved());
}

LocalFileReference::LocalFileReference(const string& url)
{
	init(url);
}

LocalFileReference::LocalFileReference(File& file)
{
	init(file);
}

void LocalFileReference::init(const string& url)
{
	File file(url);
	init(file);
}

void LocalFileReference::init(File& file)
{
	tmpFile = file.getLocalReference();
	if (!tmpFile.empty()) {
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
	auto fp = FileOperations::openUniqueFile(tmpDir, tmpFile);
	if (!fp) {
		throw FileException("Couldn't create temp file");
	}

	// write temp file
	size_t size;
	const byte* buf = file.mmap(size);
	if (fwrite(buf, 1, size, fp.get()) != size) {
		throw FileException("Couldn't write temp file");
	}
}

LocalFileReference::~LocalFileReference()
{
	if (!tmpDir.empty()) {
		FileOperations::unlink(tmpFile);
		// it's possible the directory is not empty, in that case
		// the following function will fail, we ignore that error
		FileOperations::rmdir(tmpDir);
	}
}

const string LocalFileReference::getFilename() const
{
	assert(!tmpFile.empty());
	return tmpFile;
}

} // namespace openmsx
