// $Id$
//
// LoadFile mixin, implementation

#include <iostream>
#include <fstream>
#include <string>

#include "LoadFile.hh"

void LoadFile::loadFile(byte** memoryBank, int fileSize)
{
	// TODO load from "ROM-directory"
	if (!(*memoryBank = new byte[fileSize]))
		PRT_ERROR("Couldn't allocate enough memory");
	std::string filename = LoadFileGetConfigDevice()->getParameter("filename");
	int offset = LoadFileGetConfigDevice()->getParameterAsInt("skip_headerbytes");
	PRT_DEBUG("Loading file " << filename << " ...");
#ifdef HAVE_FSTREAM_TEMPL
	std::ifstream<byte> file(filename.c_str());
#else
	std::ifstream file(filename.c_str());
#endif
	file.seekg(offset);
	file.read(*memoryBank, fileSize);
	if (file.fail())
		PRT_ERROR("Error reading " << filename);
	file.close();
}
