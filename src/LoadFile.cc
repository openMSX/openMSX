// $Id$
//
// LoadFile mixin, implementation

#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include "msxexception.hh"
#include "LoadFile.hh"

void LoadFile::loadFile(byte** memoryBank, int fileSize)
{
	FILETYPE* file = openFile();
	readFile(file, fileSize, memoryBank);
}

int LoadFile::loadFile(byte** memoryBank)
{
	FILETYPE* file = openFile();
	file->seekg(0,std::ios::end);
	int fileSize=file->tellg();
	file->seekg(0,std::ios::beg);
	PRT_DEBUG("rom size is determined to be " << fileSize);
	readFile(file, fileSize, memoryBank);
	return fileSize;
}

FILETYPE* LoadFile::openFile()
{
	// TODO load from "ROM-directory" (temp "cd" to dir)
	std::string filename = getDeviceConfig()->getParameter("filename");
	PRT_DEBUG("Loading file " << filename << " ...");
	return new FILETYPE(filename.c_str());
}

void LoadFile::readFile(FILETYPE* file, int fileSize, byte** memoryBank)
{
	if (!(*memoryBank = new byte[fileSize]))
		PRT_ERROR("Couldn't allocate enough memory");
	try {
		int offset= getDeviceConfig()->getParameterAsInt("skip_headerbytes");
		file->seekg(offset);
	} catch(MSXConfig::Exception e) {
		// no offset specified
	}
	file->read(*memoryBank, fileSize);
	if (file->fail())
		PRT_ERROR("Error reading " << file);
	file->close();
	// also patch the file if needed:
	patchFile(*memoryBank, fileSize);
}

void LoadFile::patchFile(byte* memoryBank, int size)
{
	/*
	 * example:
	 * <parameter name="0x0010" class="patch">0xED</parameter>
	 */
	std::list<const MSXConfig::Config::Parameter*> parameters =
		getDeviceConfig()->getParametersWithClass("patch");
	std::list<const MSXConfig::Config::Parameter*>::const_iterator i =
		parameters.begin();
	for ( /**/ ; i!=parameters.end(); i++)
	{
		int offset = strtol((*i)->name.c_str(),0,0);
		int value  = (*i)->getAsInt();
		if (offset >= size)
		{
			PRT_DEBUG("Ignoring illegal ROM patch-offset: " << offset);
		}
		else
		{
			PRT_DEBUG("Patching ROM[" << (*i)->name << "]=" << (*i)->value);
			memoryBank[offset] = value;
		}
	}
}
