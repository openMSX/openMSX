// $Id$

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <cassert>
#include "File.hh"
#include "FileOperations.hh"
#include "CompressedFileAdapter.hh"
#ifdef	__WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


using std::ostringstream;


namespace openmsx {

int CompressedFileAdapter::tmpCount = 0;
string CompressedFileAdapter::tmpDir;


CompressedFileAdapter::CompressedFileAdapter(FileBase* file_)
	throw()
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
	delete file;
}


void CompressedFileAdapter::read(byte* buffer, unsigned num) throw()
{
	memcpy(buffer, &buf[pos], num);
}

void CompressedFileAdapter::write(const byte* buffer, unsigned num)
	throw(FileException)
{
	throw FileException("Writing to compressed files not yet supported");
}

unsigned CompressedFileAdapter::getSize() throw()
{
	return size;
}

void CompressedFileAdapter::seek(unsigned newpos) throw()
{
	pos = newpos;
}

unsigned CompressedFileAdapter::getPos() throw()
{
	return pos;
}

void CompressedFileAdapter::truncate(unsigned size) throw(FileException)
{
	throw FileException("Truncating compressed files not yet supported.");
}

const string CompressedFileAdapter::getURL() const throw(FileException)
{
	return file->getURL();
}

const string CompressedFileAdapter::getLocalName() throw(FileException)
{
	if (!localName) {
		// create temp dir
		if (tmpCount == 0) {
#ifdef	__WIN32__
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
#ifdef	__WIN32__
		char tmpname[MAX_PATH];
		if (!GetTempFileNameA(tmpDir.c_str(), "openmsx", 0, tmpname)) {
			throw FileException("Coundn't get temp file name");
		}
		localName = strdup(tmpname);
		FILE *file = fopen(localName, "w");
#else
		string tmp = tmpDir + "/XXXXXX";
		localName = strdup(tmp.c_str());
		int fd = mkstemp(localName);
		FILE* file = fdopen(fd, "w");
#endif
		
		// write temp file
		if (!file) {
			throw FileException("Couldn't create temp file");
		}
		fwrite(buf, 1, size, file);
		fclose(file);
		++tmpCount;
	}
	return FileOperations::getConventionalPath(localName);
}

bool CompressedFileAdapter::isReadOnly() const throw()
{
	return true;
}

} // namespace openmsx
