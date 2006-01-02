// $Id$

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <cassert>
#include "FileOperations.hh"
#include "FileException.hh"
#include "CompressedFileAdapter.hh"
#ifdef	_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using std::auto_ptr;
using std::ostringstream;
using std::string;

namespace openmsx {

int CompressedFileAdapter::tmpCount = 0;
string CompressedFileAdapter::tmpDir;


CompressedFileAdapter::CompressedFileAdapter(auto_ptr<FileBase> file_)
	: file(file_), buf(0), pos(0), localName(0)
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
	if (localName) {
		unlink(localName);
		free(localName);
		--tmpCount;
		if (tmpCount == 0) {
			rmdir(tmpDir.c_str());
		}
	}

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

void CompressedFileAdapter::read(byte* buffer, unsigned num)
{
	fillBuffer();
	memcpy(buffer, &buf[pos], num);
	pos += num;
}

void CompressedFileAdapter::write(const byte* /*buffer*/, unsigned /*num*/)
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

const string CompressedFileAdapter::getLocalName()
{
	fillBuffer();
	if (!localName) {
		// create temp dir
		if (tmpCount == 0) {
#ifdef	_WIN32
			char tmppath[MAX_PATH];
			if (!GetTempPathA(MAX_PATH, tmppath)) {
				throw FileException("Coundn't get temp file path");
			}
			tmpDir = string(tmppath) + "\\openmsx";
#else
			ostringstream os;
			os << "/tmp/openmsx." << getpid();
			tmpDir = os.str();
#endif
			FileOperations::mkdirp(tmpDir);
		}

		// create temp file
#ifdef	_WIN32
		char tmpname[MAX_PATH];
		if (!GetTempFileNameA(tmpDir.c_str(), "openmsx", 0, tmpname)) {
			throw FileException("Coundn't get temp file name");
		}
		localName = strdup(tmpname);
		FILE* fp = fopen(localName, "w");
#else
		string tmp = tmpDir + "/XXXXXX";
		localName = strdup(tmp.c_str());
		int fd = mkstemp(localName);
		FILE* fp = fdopen(fd, "w");
#endif

		// write temp file
		if (!fp) {
			throw FileException("Couldn't create temp file");
		}
		fwrite(buf, 1, size, fp);
		fclose(fp);
		++tmpCount;
	}
	return FileOperations::getConventionalPath(localName);
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
