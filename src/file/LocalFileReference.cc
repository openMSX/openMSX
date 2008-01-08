// $Id$

#include "LocalFileReference.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using std::string;

namespace openmsx {

LocalFileReference::LocalFileReference(const string& url)
{
	File file(url);
	tmpFile = file.getLocalReference();
	if (!tmpFile.empty()) {
		// file is backed on the (local) filesystem,
		// we can simply use the path to that file
		assert(tmpDir.empty()); // no need to delete file/dir later
		return;
	}

	// create temp dir
#ifdef _WIN32
	char tmppath[MAX_PATH];
	if (!GetTempPathA(MAX_PATH, tmppath)) {
		throw FileException("Coundn't get temp file path");
	}
	tmpDir = string(tmppath) + "\\openmsx";
#else
	tmpDir = "/tmp/openmsx." + StringOp::toString(getpid());
#endif
	// it's possible this directory already exists, in that case the
	// following function does nothing
	FileOperations::mkdirp(tmpDir);

	// create temp file
#ifdef _WIN32
	char tmpname[MAX_PATH];
	if (!GetTempFileNameA(tmpDir.c_str(), "openmsx", 0, tmpname)) {
		throw FileException("Coundn't get temp file name");
	}
	tmpFile = tmpname;
	FILE* fp = fopen(tmpFile.c_str(), "wb");
#else
	tmpFile = tmpDir + "/XXXXXX";
	int fd = mkstemp(const_cast<char*>(tmpFile.c_str()));
	if (fd == -1) {
		throw FileException("Coundn't get temp file name");
	}
	FILE* fp = fdopen(fd, "wb");
#endif
	if (!fp) {
		throw FileException("Couldn't create temp file");
	}

	// write temp file
	byte* buf = file.mmap();
	unsigned size = file.getSize();
	if (fwrite(buf, 1, size, fp) != size) {
		throw FileException("Couldn't write temp file");
	}
	fclose(fp);
}

LocalFileReference::~LocalFileReference()
{
	if (!tmpDir.empty()) {
		unlink(tmpFile.c_str());
		// it's possible the directory is not empty, in that case
		// the following function will fail, we ignore that error
		rmdir(tmpDir.c_str());
	}
}

const string LocalFileReference::getFilename() const
{
	assert(!tmpFile.empty());
	return tmpFile;
}

} // namespace openmsx
