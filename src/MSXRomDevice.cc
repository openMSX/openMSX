// $Id$

#include <string>
#include "MSXConfig.hh"
#include "MSXRomDevice.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomPatchInterface.hh"
#include "File.hh"


MSXRomDevice::MSXRomDevice(Device* config, const EmuTime &time)
{
	if (config->hasParameter("filename")) {
		std::string filename = config->getParameter("filename");
		read(config, filename, time);
	} else {
		size = 0;
		file = NULL;
	}
}

MSXRomDevice::MSXRomDevice(Device* config, const std::string &filename,
                           const EmuTime &time)
{
	read(config, filename, time);	// TODO config
}

void MSXRomDevice::read(Device* config,
                        const std::string &filename, const EmuTime &time)
{
	// open file
	const std::string &context = config->getContext();
	file = new File(context, filename);
	
	// get filesize
	int fileSize;
	if (config && config->hasParameter("filesize") &&
	    config->getParameter("filesize") != "auto") {
		fileSize = config->getParameterAsInt("filesize");
	} else {
		fileSize = file->size();
	}
	
	// get offset
	int offset = 0;
	if (config && config->hasParameter("skip_headerbytes")) {
		offset = config->getParameterAsInt("skip_headerbytes");
	}

	// some checks 
	size = fileSize - offset;
	if (size <= 0) {
		PRT_ERROR("Offset greater than filesize");
	}
	
	// read file
	try {
		byte *tmp = file->mmap();
		rom = tmp + offset;
	} catch (FileException &e) {
		PRT_ERROR("Error reading ROM image: " << filename);
	}

	if (!config) {
		return;
	}
	// for each patchcode parameter, construct apropriate patch
	// object and register it at MSXCPUInterface
	std::list<Config::Parameter*>* parameters =
		config->getParametersWithClass("patchcode");
	std::list<Config::Parameter*>::const_iterator it;
	for (it=parameters->begin(); it!=parameters->end(); it++) {
		MSXRomPatchInterface* patchInterface;
		if ((*it)->value == "MSXDiskRomPatch") {
			patchInterface = new MSXDiskRomPatch(time);
		} else if ((*it)->value == "MSXTapePatch") {
			patchInterface = new MSXTapePatch();
		} else {
			PRT_ERROR("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXCPUInterface::instance()->registerInterface(patchInterface);
	}
	config->getParametersWithClassClean(parameters);
	
	// also patch the file if needed:
	std::list<Config::Parameter*>* parameters2 =
		config->getParametersWithClass("patch");
	std::list<Config::Parameter*>::const_iterator i;
	for (i=parameters2->begin(); i!=parameters2->end(); i++) {
		unsigned int offset = strtol((*i)->name.c_str(), 0, 0);
		int value  = (*i)->getAsInt();
		if (offset >= size) {
			PRT_DEBUG("Ignoring illegal ROM patch-offset: " << offset);
		} else {
			PRT_DEBUG("Patching ROM[" << (*i)->name << "]=" << (*i)->value);
			rom[offset] = value;
		}
	}
	config->getParametersWithClassClean(parameters2);
}


MSXRomDevice::~MSXRomDevice()
{
	std::list<MSXRomPatchInterface*>::iterator i;
	for (i=romPatchInterfaces.begin(); i!=romPatchInterfaces.end(); i++) {
		MSXCPUInterface::instance()->unregisterInterface(*i);
		delete (*i);
	}
	if (file) {
		file->munmap();
		delete file;
	}
}
