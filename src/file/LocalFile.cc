// $Id$

#include <unistd.h>
#include <sys/mman.h>
#include "LocalFile.hh"
#include "File.hh"

LocalFile::LocalFile(const std::string &filename_, int options)
	: filename(filename_)
{
	if (filename[0] == '~') {
		filename = std::string(getenv("HOME")) + filename.substr(1);
	}
	PRT_DEBUG("LocalFile: " << filename);
	const char* name = filename.c_str(); 
	if (options & TRUNCATE) {
		// open file read/write truncated
		file = fopen(name, "wb+");
	} else {
		// open file read/write
		file = fopen(name, "rb+");
		if (!file) {
			// if that fails try read only
			file = fopen(name, "rb");
		}
	}
	if (!file) {
		throw FileException("Error opening file " + filename);
	}
}

LocalFile::~LocalFile()
{
	fclose(file);
}

void LocalFile::read(byte* buffer, int num)
{
	fread(buffer, 1, num, file);
	if (ferror(file)) {
		throw FileException("Error reading file");
	}
}

void LocalFile::write(const byte* buffer, int num)
{
	fwrite(buffer, 1, num, file);
	if (ferror(file)) {
		throw FileException("Error writing file");
	}
}

byte* LocalFile::mmap(bool write)
{
	if (!mmem) {
		int flags = write ? MAP_SHARED : MAP_PRIVATE;
		mmem = (byte*)::mmap(0, size(), PROT_READ | PROT_WRITE,
		                     flags, fileno(file), 0);
		if ((int)mmem == -1) {
			throw FileException("Error mmapping file");
		}
	}
	return mmem;
}

void LocalFile::munmap()
{
	if (mmem) {
		::munmap(mmem, size());
		mmem = NULL;
	}
}

int LocalFile::size()
{
	long pos = ftell(file);
	fseek(file, 0, SEEK_END);
	int size = (int)ftell(file);
	fseek(file, pos, SEEK_SET);
	return size;
}

void LocalFile::seek(int pos)
{
	fseek(file, pos, SEEK_SET);
	if (ferror(file)) {
		throw FileException("Error seeking file");
	}
}

int LocalFile::pos()
{
	return (int)ftell(file);
}

const std::string& LocalFile::getLocalName()
{
	return filename;
}
