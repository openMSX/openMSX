// $Id$
//
// LoadFile mixin, implementation

#include <fstream>
#include <string>
#include <list>
#include "MSXException.hh"
#include "LoadFile.hh"


void LoadFile::loadFile(byte** memoryBank, int fileSize)
{
	IFILETYPE* file = openFile();
	readFile(file, fileSize, memoryBank);
}

int LoadFile::loadFile(byte** memoryBank)
{
	IFILETYPE* file = openFile();
	file->seekg(0,std::ios::end);
	int fileSize=file->tellg();
	file->seekg(0,std::ios::beg);
	PRT_DEBUG("rom size is determined to be " << fileSize);
	readFile(file, fileSize, memoryBank);
	return fileSize;
}

IFILETYPE* LoadFile::openFile()
{
	std::string filename = getDeviceConfig()->getParameter("filename");
	PRT_DEBUG("Loading file " << filename << " ...");
	// FileOpener loads from "ROM-directory" if needed
	return FileOpener::openFileRO(filename);
}

void LoadFile::readFile(IFILETYPE* file, int fileSize, byte** memoryBank)
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
	std::list<MSXConfig::Config::Parameter*>* parameters =
		getDeviceConfig()->getParametersWithClass("patch");
	std::list<MSXConfig::Config::Parameter*>::const_iterator i =
		parameters->begin();
	for ( /**/ ; i!=parameters->end(); i++)
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
	getDeviceConfig()->getParametersWithClassClean(parameters);
}
