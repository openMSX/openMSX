// $Id$

#include <cassert>
#include <algorithm>
#include "FileBase.hh"

using std::string;

namespace openmsx {

FileBase::FileBase()
	: mmem(NULL)
{
}

FileBase::~FileBase()
{
	munmap();
}

byte* FileBase::mmap(bool writeBack)
{
	if (!mmem) {
		mmapWrite = writeBack;
		mmapSize = getSize();
		mmem = new byte[mmapSize];
		read(mmem, mmapSize);
	}
	return mmem;
}

void FileBase::munmap()
{
	if (mmem) {
		if (mmapWrite) {
			seek(0);
			write(mmem, mmapSize);
		}
		delete[] mmem;
		mmem = NULL;
	}
}

void FileBase::truncate(unsigned size)
{
	int grow = size - getSize();
	if (grow < 0) {
		PRT_DEBUG("Default truncate() can't shrink file!");
		return;
	}
	const int BUF_SIZE = 4096;
	byte buf[BUF_SIZE];
	memset(buf, 0, BUF_SIZE);
	while (grow > 0) {
		write(buf, std::min(BUF_SIZE, grow));
		grow -= BUF_SIZE;
	}
} 

const string FileBase::getOriginalName()
{
	// default implementation just returns filename portion of URL
	string url = getURL();
	string::size_type pos = url.find_last_of('/');
	return (pos == string::npos) ? url : url.substr(pos + 1);
}

} // namespace openmsx
