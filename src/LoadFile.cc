// $Id$
//
// LoadFile mixin, implementation

#include <iostream>
#include <fstream>
#include <string>
#include <list>

#include "LoadFile.hh"

// It was easier to integrate the filesize determination in the greater codepart.
// To clean this up you should need to duplicate a lot of code or split it up in
// more (stupid) smaller parts.

void LoadFile::loadFile(byte** memoryBank, int fileSize)
{
   int tmp=fileSize;
   loadUnknownFile(memoryBank,tmp);
}

void LoadFile::loadUnknownFile(byte** memoryBank, int &fileSize)
{
	std::string filename = getDeviceConfig()->getParameter("filename");
	int offset = getDeviceConfig()->getParameterAsInt("skip_headerbytes");
	PRT_DEBUG("Loading file " << filename << " ...");
	
	// TODO load from "ROM-directory"
        // We need to adjust filename to include the path where it can be found !!
	

#ifdef HAVE_FSTREAM_TEMPL
	std::ifstream<byte> file(filename.c_str());
#else
	std::ifstream file(filename.c_str());
#endif
	// Determine filesize automatically if needed
	if ( fileSize == 0 ){
		 file.seekg(0,ios::end);
		 fileSize=file.tellg();
		 PRT_DEBUG("rom size is determined to be " << fileSize);
	};

	if (!(*memoryBank = new byte[fileSize]))
		PRT_ERROR("Couldn't allocate enough memory");
	file.seekg(offset);
	file.read(*memoryBank, fileSize);
	if (file.fail())
		PRT_ERROR("Error reading " << filename);
	file.close();
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
