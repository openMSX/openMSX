// $Id$

#include "LocalFile.hh"
#include "File.hh"

LocalFile::LocalFile(const std::string &filename, int options)
{
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
		throw FileException("Error opening file" + filename);
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
