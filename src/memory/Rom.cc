// $Id$

#include <string>
#include <sstream>
#include "xmlx.hh"
#include "Rom.hh"
#include "RomInfo.hh"
#include "MSXCPUInterface.hh"
#include "File.hh"
#include "FileContext.hh"
#include "PanasonicMemory.hh"
#include "StringOp.hh"
#include "Debugger.hh"
#include "sha1.hh"
#include "CliCommOutput.hh"
#include "FilePool.hh"
#include "ConfigException.hh"

namespace openmsx {

Rom::Rom(const string& name_, const string& description_,
         const XMLElement& config)
	: name(name_), description(description_)
{
	init(config.getChild("rom"));
}

Rom::Rom(const string& name_, const string& description_,
         const XMLElement& config, const string& id)
	: name(name_), description(description_)
{
	XMLElement::Children romConfigs;
	config.getChildren("rom", romConfigs);
	for (XMLElement::Children::const_iterator it = romConfigs.begin();
	     it != romConfigs.end(); ++it) {
		if ((*it)->getId() == id) {
			init(**it);
			return;
		}
	}
	throw ConfigException("ROM tag \"" + id + "\" missing.");
}

void Rom::readIPS(const XMLElement& config, const string& filename)
{
	// open file
	try {
		file.reset(new File(config.getFileContext().resolve(filename)));
	} catch (FileException& e) {
		throw FatalError("Error reading IPS: " + filename);
	}

	byte buf[5];
	file->read(buf,5u);
	if ( strncmp((const char*)buf,"PATCH",5) != 0 ) {
		throw FatalError("Not a valid IPS: " + filename);
	}
	try{
	  	byte* writable_rom = const_cast<byte*>(rom);
		file->read(buf,3u);
		while ( strncmp((const char*)buf,"EOF",3) != 0 ) {
		  	int offset=256*256*buf[0]+256*buf[1]+buf[2];
			cout << "offset:" << offset <<endl;
			file->read(buf,2u);
			int length=256*buf[0]+buf[1];
			cout << "length:" << length <<endl;
			if (length == 0){
				// RLE encoded
				file->read(buf,3u);
				length=256*buf[0]+buf[1];
				cout << "RLE length:" << length <<endl;
				cout << "RLE byte:" << (int)buf[2] <<endl;
				memset(writable_rom+offset,buf[2],length);
			} else {
				//Patch bytes
				file->read(writable_rom+offset,length);
			}
			file->read(buf,3u);
		}
	} catch (FileException& e) {
		throw FatalError("Error reading IPS: " + filename);
	}
}


void Rom::init(const XMLElement& config)
{
  	cout << config.dump() <<endl;
	XMLElement::Children sums;
	config.getChildren("sha1", sums);
	const XMLElement* filenameElem = config.findChild("filename");
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
			                 config.getId() + "\".");
		}
		read(config, filename);

	} else if (config.findChild("firstblock")) {
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

	if (size != 0 ) {
		const XMLElement* ipsElem = config.findChild("ips");
		if (ipsElem) {
			const XMLElement& ipsfilenameElem = ipsElem->getChild("filename");
			readIPS(config, ipsfilenameElem.getData() );
		}
	}
	info = RomInfo::fetchRomInfo(*this);

	// TODO fix this, this is a hack that depends heavily on MSXRomCLI.cc
	if (!info->getTitle().empty() &&
	    ((name.size() >= 6) && (name.substr(0, 6) == "MSXRom"))) {
		name = XMLElement::makeUnique(info->getTitle());
	}

	if (size) {
		Debugger::instance().registerDebuggable(name, *this);
	}
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
			"SHA1 sum for '" + config.getId() +
			"' does not match with sum of '" + filename +
			"'.");
	}
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

Rom::~Rom()
{
	if (size) {
		Debugger::instance().unregisterDebuggable(name, *this);
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
		sha1sum = sha1.hex_digest();
	}
	return sha1sum;
}

} // namespace openmsx
