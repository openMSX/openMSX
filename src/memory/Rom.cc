// $Id$

#include <string>
#include <sstream>
#include "xmlx.hh"
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
#include "sha1.hh"
#include "CliCommOutput.hh"
#include "FilePool.hh"

namespace openmsx {

Rom::Rom(const string& name_, const string& description_, const XMLElement& config)
	: name(name_), description(description_)
{
	XMLElement::Children sums;
	config.getChildren("sha1", sums);
	const XMLElement* filenameElem = config.getChild("filename");
	if (!sums.empty() || filenameElem) {
		// file specified with SHA1 or filename
		string filename;
		if (filenameElem) {
			filename = filenameElem->getData();
		}
		for (XMLElement::Children::const_iterator it = sums.begin();
		     it != sums.end(); ++it) {
			const string& sha1 = (*it)->getData();
			string sha1File = FilePool::instance().getFile(sha1);
			if (!sha1File.empty()) {
				filename = sha1File;
				sha1sum = sha1;
				break;
			}
		}
		if (filename.empty()) {
			throw FatalError("Couldn't find ROM file for \"" +
			                 config.getAttribute("id") + "\".");
		}
		read(config, filename);

	} else if (config.getChild("firstblock")) {
		// part of the TurboR main ROM
		int first = config.getChildDataAsInt("firstblock");
		int last  = config.getChildDataAsInt("lastblock");
		size = (last - first + 1) * 0x2000;
		rom = PanasonicMemory::instance().getRomBlock(first);
		assert(last >= first);
		assert(rom);

	} else {
		// Assumption: this only happens for an empty SCC
		size = 0;
	}

	init(config);
}

Rom::Rom(const string& name_, const string& description_, const XMLElement& config,
         const string& filename)
	: name(name_), description(description_)
{
	read(config, filename);	// TODO config
	init(config);
}

void Rom::read(const XMLElement& config, const string& filename)
{
	// open file
	try {
		file.reset(new File(config.getFileContext().resolve(filename)));
	} catch (FileException& e) {
		throw FatalError("Error reading ROM: " + filename);
	}
	
	// get filesize
	int fileSize;
	string fileSizeStr = config.getChildData("filesize", "auto");
	if (fileSizeStr == "auto") {
		fileSize = file->getSize();
	} else {
		fileSize = StringOp::stringToInt(fileSizeStr);
	}
	
	// get offset
	int offset = config.getChildDataAsInt("skip_headerbytes", 0);
	if (fileSize <= offset) {
		throw FatalError("Offset greater than filesize");
	}
	size = fileSize - offset;
	
	// read file
	byte* tmp = 0;	// avoid warning
	try {
		tmp = file->mmap() + offset;
		rom = tmp;
	} catch (FileException &e) {
		throw FatalError("Error reading ROM image: " + filename);
	}

	// verify SHA1
	if (!checkSHA1(config)) {
		CliCommOutput::instance().printWarning(
			"SHA1 sum for '" + config.getAttribute("id") +
			"' does not match with sum of '" + filename +
			"'.");
	}
	
	patch(config);
}

bool Rom::checkSHA1(const XMLElement& config)
{
	const string& sha1sum = getSHA1Sum();
	XMLElement::Children sums;
	config.getChildren("sha1", sums);
	if (sums.empty()) {
		return true;
	}
	for (XMLElement::Children::const_iterator it = sums.begin();
	     it != sums.end(); ++it) {
		if ((*it)->getData() == sha1sum) {
			return true;
		}
	}
	return false;
}

void Rom::patch(const XMLElement& config)
{
	// for each patchcode parameter, construct apropriate patch
	// object and register it at MSXCPUInterface
	XMLElement::Children patchCodes;
	config.getChildren("patchcode", patchCodes);
	for (XMLElement::Children::const_iterator it = patchCodes.begin();
	     it != patchCodes.end(); ++it) {
		MSXRomPatchInterface* patchInterface;
		string name = (*it)->getData();
		if (name == "MSXDiskRomPatch") {
			patchInterface = new MSXDiskRomPatch();
		} else if (name == "MSXTapePatch") {
			patchInterface = new MSXTapePatch();
		} else {
			throw FatalError("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXCPUInterface::instance().registerInterface(patchInterface);
	}
	
	// also patch the file if needed:
	byte* tmp = const_cast<byte*>(rom);
	XMLElement::Children patches;
	config.getChildren("patch", patches);
	for (XMLElement::Children::const_iterator it = patches.begin();
	     it != patches.end(); ++it) {
		unsigned addr = StringOp::stringToInt((*it)->getAttribute("addr"));
		unsigned val  = StringOp::stringToInt((*it)->getAttribute("val"));
		if (addr >= size) {
			ostringstream out;
			out << "Illegal ROM patch-offset: 0x" << hex << addr;
			throw FatalError(out.str());
		}
		tmp[addr] = val;
	}
}

void Rom::init(const XMLElement& config)
{
	info = RomInfo::fetchRomInfo(*this, config);

	// TODO fix this, this is a hack that depends heavily on MSXRomCLI.cc
	if (!info->getTitle().empty() && (name.substr(0, 6) == "MSXRom")) {
		char ps = name[6];
		char ss = name[8];
		name = info->getTitle() + " in slot " + ps + '-' + ss;
	}

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
	if (file.get()) {
		file->munmap();
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

void Rom::write(unsigned /*address*/, byte /*value*/)
{
	// ignore
}

const string& Rom::getName() const
{
	return name;
}

const string& Rom::getSHA1Sum() const
{
	if (sha1sum.empty()) {
		SHA1 sha1;
		sha1.update(rom, size);
		sha1.finalize();
		sha1sum = sha1.hex_digest();
	}
	return sha1sum;
}

} // namespace openmsx
