// $Id$
//

#include <fstream>
#include <string>
#include <list>
#include "MSXException.hh"
#include "MSXRomDevice.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXMotherBoard.hh"
#include "MSXRomPatchInterface.hh"

MSXRomDevice::MSXRomDevice(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	handleRomPatchInterfaces();
}

MSXRomDevice::~MSXRomDevice()
{
	PRT_DEBUG("Deleting a MSXRomDevice");
	
	std::list<MSXRomPatchInterface*>::iterator i;
	for (i=romPatchInterfaces.begin(); i!=romPatchInterfaces.end(); i++) {
		delete (*i);
	}
	delete [] memoryBank;
}

void MSXRomDevice::handleRomPatchInterfaces()
{
	// for each patchcode parameter, construct apropriate patch
	// object and register it at MSXMotherBoard
	std::list<MSXConfig::Config::Parameter*>* parameters =
		deviceConfig->getParametersWithClass("patchcode");
	std::list<MSXConfig::Config::Parameter*>::const_iterator i =
		parameters->begin();
	for ( /**/ ; i!=parameters->end(); i++) {
		MSXRomPatchInterface* patchInterface;
		if ((*i)->value == "MSXDiskRomPatch") {
			PRT_DEBUG("Creating MSXDiskRomPatch");
			patchInterface = new MSXDiskRomPatch();
		} else if ((*i)->value == "MSXTapePatch") {
			PRT_DEBUG("Creating MSXTapePatch");
			patchInterface = new MSXTapePatch();
		} else {
			PRT_ERROR("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXMotherBoard::instance()->registerInterface(patchInterface);
	}
	deviceConfig->getParametersWithClassClean(parameters);
}


void MSXRomDevice::loadFile(byte** memoryBank, int fileSize)
{
	IFILETYPE* file = openFile();
	readFile(file, fileSize, memoryBank);
	delete file;
}

int MSXRomDevice::loadFile(byte** memoryBank)
{
	IFILETYPE* file = openFile();
	file->seekg(0,std::ios::end);
	int fileSize=file->tellg();
	file->seekg(0,std::ios::beg);
	PRT_DEBUG("rom size is determined to be " << fileSize);
	readFile(file, fileSize, memoryBank);
	delete file;
	return fileSize;
}

IFILETYPE* MSXRomDevice::openFile()
{
	std::string filename = deviceConfig->getParameter("filename");
	PRT_DEBUG("Loading file " << filename << " ...");
	// FileOpener loads from "ROM-directory" if needed
	return FileOpener::openFileRO(filename);
}

void MSXRomDevice::readFile(IFILETYPE* file, int fileSize, byte** memoryBank)
{
	if (!(*memoryBank = new byte[fileSize]))
		PRT_ERROR("Couldn't allocate enough memory");
	try {
		int offset= deviceConfig->getParameterAsInt("skip_headerbytes");
		file->seekg(offset);
	} catch(MSXConfig::Exception e) {
		// no offset specified
	}
	file->read((char*)*memoryBank, fileSize);
	if (file->fail()) {
		// TODO: Throw exception, so that caller can print an error
		//       message which includes the file name.
		PRT_ERROR("Error reading ROM image.");
	}
	file->close();
	// also patch the file if needed:
	patchFile(*memoryBank, fileSize);
}

void MSXRomDevice::patchFile(byte* memoryBank, int size)
{
	/*
	 * example:
	 * <parameter name="0x0010" class="patch">0xED</parameter>
	 */
	std::list<MSXConfig::Config::Parameter*>* parameters =
		deviceConfig->getParametersWithClass("patch");
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
	deviceConfig->getParametersWithClassClean(parameters);
}
