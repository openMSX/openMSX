// $Id$

#include "LocalFile.hh"
#include "File.hh"

LocalFile::LocalFile(const std::string &filename, int options)
{
	char* mode;
	if (options & TRUNCATE) {
		mode = "wb+";
	} else {
		mode = "rb+";
	}
	
	file = fopen(filename.c_str(), mode);
	if (file == NULL) {
		throw FileException("Error opening file");
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
