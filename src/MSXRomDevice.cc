// $Id$

#include <string>
#include "MSXRomDevice.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomPatchInterface.hh"


MSXRomDevice::MSXRomDevice(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	handleRomPatchInterfaces();
	loadFile();
}

MSXRomDevice::MSXRomDevice(MSXConfig::Device *config, const EmuTime &time, int fileSize)
	: MSXDevice(config, time)
{
	handleRomPatchInterfaces();
	loadFile(fileSize);
}

MSXRomDevice::~MSXRomDevice()
{
	std::list<MSXRomPatchInterface*>::iterator i;
	for (i=romPatchInterfaces.begin(); i!=romPatchInterfaces.end(); i++) {
		MSXCPUInterface::instance()->unregisterInterface(*i);
		delete (*i);
	}
	delete[] romBank;
}


void MSXRomDevice::handleRomPatchInterfaces()
{
	// for each patchcode parameter, construct apropriate patch
	// object and register it at MSXCPUInterface
	std::list<MSXConfig::Config::Parameter*>* parameters =
		deviceConfig->getParametersWithClass("patchcode");
	std::list<MSXConfig::Config::Parameter*>::const_iterator i;
	for (i=parameters->begin(); i!=parameters->end(); i++) {
		MSXRomPatchInterface* patchInterface;
		if ((*i)->value == "MSXDiskRomPatch") {
			patchInterface = new MSXDiskRomPatch();
		} else if ((*i)->value == "MSXTapePatch") {
			patchInterface = new MSXTapePatch();
		} else {
			PRT_ERROR("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXCPUInterface::instance()->registerInterface(patchInterface);
	}
	deviceConfig->getParametersWithClassClean(parameters);
}


void MSXRomDevice::loadFile()
{
	if (deviceConfig->hasParameter("filesize")) {
		const char* str = deviceConfig->getParameter("filesize").c_str();
		if (strcasecmp("auto", str) != 0) {
			loadFile(atoi(str));
			return;
		}
	}
	// autodetect filesize
	IFILETYPE* file = openFile();
	file->seekg(0, std::ios::end);
	int fileSize = file->tellg();
	file->seekg(0, std::ios::beg);
	
	readFile(file, fileSize);
	delete file;
}

void MSXRomDevice::loadFile(int fileSize)
{
	IFILETYPE* file = openFile();
	readFile(file, fileSize);
	delete file;
}

IFILETYPE* MSXRomDevice::openFile()
{
	std::string filename = deviceConfig->getParameter("filename");
	return FileOpener::openFileRO(filename);
}

void MSXRomDevice::readFile(IFILETYPE* file, int fileSize)
{
	int offset = 0;
	try {
		offset = deviceConfig->getParameterAsInt("skip_headerbytes");
		file->seekg(offset);
	} catch(MSXConfig::Exception e) {
		// no offset specified
	}
	romSize = fileSize - offset;
	if (romSize <= 0)
		PRT_ERROR("Offset greater than filesize");
	if (!(romBank = new byte[romSize]))
		PRT_ERROR("Couldn't allocate enough memory");
	file->read(romBank, romSize);
	if (file->fail()) {
		// TODO: Throw exception, so that caller can print an error
		//       message which includes the file name.
		PRT_ERROR("Error reading ROM image : " <<  deviceConfig->getParameter("filename") );
	}
	// also patch the file if needed:
	patchFile();
}

void MSXRomDevice::patchFile()
{
	/*
	 * example:
	 * <parameter name="0x0010" class="patch">0xED</parameter>
	 */
	std::list<MSXConfig::Config::Parameter*>* parameters =
		deviceConfig->getParametersWithClass("patch");
	std::list<MSXConfig::Config::Parameter*>::const_iterator i;
	for (i=parameters->begin(); i!=parameters->end(); i++) {
		int offset = strtol((*i)->name.c_str(), 0, 0);
		int value  = (*i)->getAsInt();
		if (offset >= romSize) {
			PRT_DEBUG("Ignoring illegal ROM patch-offset: " << offset);
		} else {
			PRT_DEBUG("Patching ROM[" << (*i)->name << "]=" << (*i)->value);
			romBank[offset] = value;
		}
	}
	deviceConfig->getParametersWithClassClean(parameters);
}
