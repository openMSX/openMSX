// $Id$

#include "LocalFile.hh"
#include "File.hh"
#include <ios>

LocalFile::LocalFile(const std::string &filename, int options)
{
	// first try read/write
	std::ios_base::openmode baseFlags = std::ios::in;
	std::ios_base::openmode extraFlags = std::ios::binary;
	if (options & TRUNCATE) {
		extraFlags |= std::ios::trunc | std::ios::out;
	}
	readOnly = false;
	file = new std::fstream(filename.c_str(),
		                baseFlags | extraFlags | std::ios::out);
	if (file->fail()) {
		// then try read-only
		delete file;
		readOnly = true;
		file = new std::fstream(filename.c_str(),
		                        baseFlags | extraFlags);
		if (file->fail()) {
			throw FileException("Error opening file");
		}
	}
}

LocalFile::~LocalFile()
{
	delete file;
}

void LocalFile::read(byte* buffer, int num)
{
	file->read((char*)buffer, num);
	if (file->fail()) {
		throw FileException("Error reading file");
	}
}

void LocalFile::write(const byte* buffer, int num)
{
	file->write((char*)buffer, num);
	if (file->fail()) {
		throw FileException("Error writing file");
	}
}

int LocalFile::size()
{
	int pos = file->tellg();
	file->seekg(0, std::ios::end);
	int size = file->tellg();
	file->seekg(pos, std::ios::beg);
	return size;
}

void LocalFile::seek(int pos)
{
	file->seekg(pos, std::ios::beg);
	if (!readOnly) {
		file->seekp(pos, std::ios::beg);
	}
	if (file->fail()) {
		throw FileException("Error seeking file");
	}
}

int LocalFile::pos()
{
	return file->tellg();
}
