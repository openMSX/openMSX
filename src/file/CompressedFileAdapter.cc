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
