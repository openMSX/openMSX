// $Id$

#include <string>
#include <sstream>
#include "Config.hh"
#include "Rom.hh"
#include "RomInfo.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomPatchInterface.hh"
#include "File.hh"
#include "FileContext.hh"
#include "PanasonicMemory.hh"
#include "StringOp.hh"
#include "Debugger.hh"

namespace openmsx {

Rom::Rom(const string& name_, const string& description_, Config* config)
	: name(name_), description(description_)
{
	if (config->hasParameter("filename")) {
		string filename = config->getParameter("filename");
		read(config, filename);
	} else if (config->hasParameter("firstblock")) {
		int first = config->getParameterAsInt("firstblock");
		int last  = config->getParameterAsInt("lastblock");
		size = (last - first + 1) * 0x2000;
		rom = PanasonicMemory::instance().getRomBlock(first);
		file = NULL;
		assert(last >= first);
		assert(rom);
	} else { // Assumption: this only happens for an empty SCC
		size = 0;
		file = NULL;
	}
	init(*config);
}

Rom::Rom(const string& name_, const string& description_, Config* config,
         const string& filename)
	: name(name_), description(description_)
{
	read(config, filename);	// TODO config
	init(*config);
}

void Rom::read(Config* config, const string& filename)
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
	Config::Parameters parameters;
	config->getParametersWithClass("patchcode", parameters);
	for (Config::Parameters::const_iterator it = parameters.begin();
	     it != parameters.end(); ++it) {
		MSXRomPatchInterface* patchInterface;
		if (it->second == "MSXDiskRomPatch") {
			patchInterface = new MSXDiskRomPatch();
		} else if (it->second == "MSXTapePatch") {
			patchInterface = new MSXTapePatch();
		} else {
			throw FatalError("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXCPUInterface::instance().registerInterface(patchInterface);
	}
	
	// also patch the file if needed:
	Config::Parameters parameters2;
	config->getParametersWithClass("patch", parameters2);
	for (Config::Parameters::const_iterator i = parameters2.begin();
	     i != parameters2.end(); ++i) {
		unsigned int romOffset = strtol(i->first.c_str(), 0, 0);
		int value  = StringOp::stringToInt(i->second);
		if (romOffset >= size) {
			ostringstream out;
			out << "Illegal ROM patch-offset: 0x" << hex << romOffset;
			throw FatalError(out.str());
		} else {
			PRT_DEBUG("Patching ROM[" << i->first << "]=" << i->second);
			tmp[romOffset] = value; // tmp = rom, but rom is read only
		}
	}
}

void Rom::init(const Config& config)
{
	info = RomInfo::fetchRomInfo(this, config);
	if (size) {
		Debugger::instance().registerDebuggable(name, *this);
	}
}

Rom::~Rom()
{
	if (size) {
		Debugger::instance().unregisterDebuggable(name, *this);
	}
	
	for (vector<MSXRomPatchInterface*>::const_iterator it =
	           romPatchInterfaces.begin();
	     it != romPatchInterfaces.end(); ++it) {
		MSXCPUInterface::instance().unregisterInterface(*it);
		delete (*it);
	}
	if (file) {
		file->munmap();
		delete file;
	}
}


unsigned Rom::getSize() const
{
	return size;
}

const string& Rom::getDescription() const
{
	return description;
}

byte Rom::read(unsigned address)
{
	assert(address < size);
	return rom[address];
}

void Rom::write(unsigned address, byte value)
{
	// ignore
}

} // namespace openmsx
