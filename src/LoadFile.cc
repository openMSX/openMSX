// $Id$
//
// LoadFile mixin, implementation

#include <iostream>
#include <fstream>
#include <string>
#include <list>

#include "LoadFile.hh"

void LoadFile::loadFile(byte** memoryBank, int fileSize)
{
	// TODO load from "ROM-directory"
	if (!(*memoryBank = new byte[fileSize]))
		PRT_ERROR("Couldn't allocate enough memory");
	std::string filename = getDeviceConfig()->getParameter("filename");
	int offset = getDeviceConfig()->getParameterAsInt("skip_headerbytes");
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
	std::list<const MSXConfig::Config::Parameter*>::const_iterator i=parameters.begin();
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
