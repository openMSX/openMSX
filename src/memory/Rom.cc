// $Id$

#include <string>
#include "MSXConfig.hh"
#include "Rom.hh"
#include "RomInfo.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomPatchInterface.hh"
#include "File.hh"
#include "PanasonicMemory.hh"


Rom::Rom(Device* config, const EmuTime &time)
{
	if (config->hasParameter("filename")) {
		std::string filename = config->getParameter("filename");
		read(config, filename, time);
	} else if (config->hasParameter("firstblock")) {
		int first = config->getParameterAsInt("firstblock");
		int last  = config->getParameterAsInt("lastblock");
		size = (last - first + 1) * 0x2000;
		rom = PanasonicMemory::instance()->getRomBlock(first);
		file = NULL;
		assert(last >= first);
		assert(rom);
	} else { // Assumption: this only happens for an empty SCC
		size = 0;
		file = NULL;
	}
	info = RomInfo::fetchRomInfo(this, *config);
}

Rom::Rom(Device* config, const std::string &filename,
                           const EmuTime &time)
{
	read(config, filename, time);	// TODO config
	info = RomInfo::fetchRomInfo(this, *config);
}

void Rom::read(Device* config,
                        const std::string &filename, const EmuTime &time)
{
	// open file
	file = new File(config->getContext()->resolve(filename));
	
	// get filesize
	int fileSize;
	if (config && config->hasParameter("filesize")
	&& config->getParameter("filesize") != "auto") {
		fileSize = config->getParameterAsInt("filesize");
	} else {
		fileSize = file->getSize();
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
	byte *tmp = 0;	// avoid warning
	try {
		tmp = file->mmap() + offset;
		rom = tmp;
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
		unsigned int romOffset = strtol((*i)->name.c_str(), 0, 0);
		int value  = (*i)->getAsInt();
		if (romOffset >= size) {
			PRT_INFO("Ignoring illegal ROM patch-offset: " << romOffset);
		} else {
			PRT_DEBUG("Patching ROM[" << (*i)->name << "]=" << (*i)->value);
			tmp[romOffset] = value; // tmp = rom, but rom is read only
		}
	}
	config->getParametersWithClassClean(parameters2);
}


Rom::~Rom()
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
	if (info) delete info;
}
