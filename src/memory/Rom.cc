// $Id$

#include <string>
#include <sstream>
#include "Device.hh"
#include "Rom.hh"
#include "RomInfo.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomPatchInterface.hh"
#include "File.hh"
#include "FileContext.hh"
#include "PanasonicMemory.hh"


namespace openmsx {

Rom::Rom(Device* config, const EmuTime& time)
{
	if (config->hasParameter("filename")) {
		string filename = config->getParameter("filename");
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

Rom::Rom(Device* config, const string& filename, const EmuTime& time)
{
	read(config, filename, time);	// TODO config
	info = RomInfo::fetchRomInfo(this, *config);
}

void Rom::read(Device* config, const string& filename, const EmuTime& time)
{
	// open file
	try {
		file = new File(config->getContext().resolve(filename));
	} catch (FileException& e) {
		throw FatalError("Error reading ROM: " + filename);
	}
	
	// get filesize
	int fileSize;
	if (config && config->hasParameter("filesize") &&
	    config->getParameter("filesize") != "auto") {
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
		throw FatalError("Offset greater than filesize");
	}
	
	// read file
	byte* tmp = 0;	// avoid warning
	try {
		tmp = file->mmap() + offset;
		rom = tmp;
	} catch (FileException &e) {
		throw FatalError("Error reading ROM image: " + filename);
	}

	if (!config) {
		return;
	}
	// for each patchcode parameter, construct apropriate patch
	// object and register it at MSXCPUInterface
	list<Config::Parameter*>* parameters =
		config->getParametersWithClass("patchcode");
	for (list<Config::Parameter*>::const_iterator it = parameters->begin();
	     it != parameters->end(); ++it) {
		MSXRomPatchInterface* patchInterface;
		if ((*it)->value == "MSXDiskRomPatch") {
			patchInterface = new MSXDiskRomPatch(time);
		} else if ((*it)->value == "MSXTapePatch") {
			patchInterface = new MSXTapePatch();
		} else {
			throw FatalError("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXCPUInterface::instance()->registerInterface(patchInterface);
	}
	config->getParametersWithClassClean(parameters);
	
	// also patch the file if needed:
	list<Config::Parameter*>* parameters2 =
		config->getParametersWithClass("patch");
	for (list<Config::Parameter*>::const_iterator i = parameters2->begin();
	     i != parameters2->end(); ++i) {
		unsigned int romOffset = strtol((*i)->name.c_str(), 0, 0);
		int value  = (*i)->getAsInt();
		if (romOffset >= size) {
			config->getParametersWithClassClean(parameters2);
			ostringstream out;
			out << "Ignoring illegal ROM patch-offset: 0x" << hex << romOffset;
			throw FatalError(out.str());
		} else {
			PRT_DEBUG("Patching ROM[" << (*i)->name << "]=" << (*i)->value);
			tmp[romOffset] = value; // tmp = rom, but rom is read only
		}
	}
	config->getParametersWithClassClean(parameters2);
}

Rom::~Rom()
{
	for (list<MSXRomPatchInterface*>::const_iterator it =
	           romPatchInterfaces.begin();
	     it != romPatchInterfaces.end(); ++it) {
		MSXCPUInterface::instance()->unregisterInterface(*it);
		delete (*it);
	}
	if (file) {
		file->munmap();
		delete file;
	}
	delete info;
}

} // namespace openmsx
