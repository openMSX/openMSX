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
	: file(file_), buf(0), pos(0), localName(0)
{
}

CompressedFileAdapter::~CompressedFileAdapter()
{
	if (localName) {
		unlink(FileOperations::getNativePath(localName).c_str());
		free(localName);
		--tmpCount;
		if (tmpCount == 0) {
			rmdir(FileOperations::getNativePath(tmpDir).c_str());
		}
	}

	if (buf) {
		free(buf);
	}
	delete file;
}


void CompressedFileAdapter::read(byte* buffer, int num)
{
	memcpy(buffer, &buf[pos], num);
}

void CompressedFileAdapter::write(const byte* buffer, int num)
{
	throw FileException("Writing to .gz files not yet supported");
}

int CompressedFileAdapter::getSize()
{
	return size;
}

void CompressedFileAdapter::seek(int newpos)
{
	pos = newpos;
}

int CompressedFileAdapter::getPos()
{
	return pos;
}

const string CompressedFileAdapter::getURL() const
{
	return file->getURL();
}

const string CompressedFileAdapter::getLocalName()
{
	if (localName == 0) {
#ifdef	__WIN32__
		char tmppath[MAX_PATH];
		char tmpname[MAX_PATH];
		if (tmpCount == 0) {
			ostringstream os;
			if (!GetTempPathA(MAX_PATH,tmppath)) {
			throw FileException("Coundn't get temp file path");
			}
			tmpDir = FileOperations::getConventionalPath(tmppath);
		}
		if (!GetTempFileNameA(tmppath,"openmsx",0,tmpname)) {
			throw FileException("Coundn't get temp file name");
		}
		localName = strdup(FileOperations::getConventionalPath(tmpname).c_str());
		FILE *file = fopen(FileOperations::getNativePath(tmpname).c_str(), "w");
#else
		if (tmpCount == 0) {
			ostringstream os;
			os << "/tmp/openmsx." << getpid();
			tmpDir = os.str();
			FileOperations::mkdirp(tmpDir);
		}
		string templ = tmpDir + "/XXXXXX";
		localName = strdup(templ.c_str());
		int fd = mkstemp(localName);
		FILE* file = fdopen(fd, "w");
#endif
		if (!file) {
			throw FileException("Couldn't create temp file");
		}
		fwrite(buf, 1, size, file);
		fclose(file);
		++tmpCount;
	}
	return localName;
}

bool CompressedFileAdapter::isReadOnly() const
{
	return true;
}

} // namespace openmsx
