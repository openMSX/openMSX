// $Id$

#include "Rom.hh"
#include "XMLElement.hh"
#include "RomInfo.hh"
#include "RomDatabase.hh"
#include "File.hh"
#include "Filename.hh"
#include "FileException.hh"
#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"
#include "Debuggable.hh"
#include "CliComm.hh"
#include "FilePool.hh"
#include "ConfigException.hh"
#include "EmptyPatch.hh"
#include "IPSPatch.hh"
#include "StringOp.hh"
#include "sha1.hh"
#include <sstream>
#include <cstring>

using std::string;
using std::auto_ptr;

namespace openmsx {

class RomDebuggable : public Debuggable
{
public:
	RomDebuggable(Debugger& debugger, Rom& rom);
	~RomDebuggable();
	virtual unsigned getSize() const;
	virtual const std::string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	Debugger& debugger;
	Rom& rom;
};


Rom::Rom(MSXMotherBoard& motherBoard_, const string& name_,
         const string& description_, const XMLElement& config)
	: motherBoard(motherBoard_), name(name_), description(description_)
{
	init(motherBoard.getGlobalCliComm(), config.getChild("rom"));
}

Rom::Rom(MSXMotherBoard& motherBoard_, const string& name_,
         const string& description_, const XMLElement& config,
         const string& id)
	: motherBoard(motherBoard_), name(name_), description(description_)
{
	XMLElement::Children romConfigs;
	config.getChildren("rom", romConfigs);
	for (XMLElement::Children::const_iterator it = romConfigs.begin();
	     it != romConfigs.end(); ++it) {
		if ((*it)->getId() == id) {
			init(motherBoard.getGlobalCliComm(), **it);
			return;
		}
	}
	throw ConfigException("ROM tag \"" + id + "\" missing.");
}

void Rom::init(CliComm& cliComm, const XMLElement& config)
{
	CommandController& controller = motherBoard.getCommandController();

	XMLElement::Children sums;
	config.getChildren("sha1", sums);
	const XMLElement* resolvedFilenameElem = config.findChild("resolvedFilename");
	const XMLElement* filenameElem         = config.findChild("filename");
	if (resolvedFilenameElem || !sums.empty() || filenameElem) {
		// first try already resolved filename ..
		if (resolvedFilenameElem) {
			try {
				file.reset(new File(resolvedFilenameElem->getData()));
			} catch (FileException&) {
				// ignore
			}
		}
		// .. then try based on SHA1 ..
		if (!file.get()) {
			for (XMLElement::Children::const_iterator it = sums.begin();
			     it != sums.end(); ++it) {
				const string& sha1 = (*it)->getData();
				file = motherBoard.getFilePool().getFile(sha1);
				if (file.get()) {
					// avoid recalculating same sha1 later
					originalSha1 = sha1;
					break;
				}
			}
		}
		// .. and then try filename as originally given by user
		if (!file.get()) {
			if (filenameElem) {
				string name = filenameElem->getData();
				try {
					Filename filename(name,
					                  config.getFileContext(),
					                  controller);
					file.reset(new File(filename));
				} catch (FileException&) {
					throw MSXException("Error reading ROM: " +
					                   name);
				}
			} else {
				throw MSXException("Couldn't find ROM file for \"" +
				                   config.getId() + "\".");
			}
		}
		read(config);

	} else if (config.findChild("firstblock")) {
		// part of the TurboR main ROM
		int first = config.getChildDataAsInt("firstblock");
		int last  = config.getChildDataAsInt("lastblock");
		size = (last - first + 1) * 0x2000;
		rom = motherBoard.getPanasonicMemory().getRomRange(first, last);
		assert(rom);

	} else {
		// for an empty SCC the <size> tag is missing, so take 0
		// for MegaFlashRomSCC the <size> tag is used to specify
		// the size of the mapper (and you don't care about initial
		// content)
		size = config.getChildDataAsInt("size", 0) * 1024; // in kb
		extendedRom.assign(size, 0xff);
		rom = size ? &extendedRom[0] : NULL;
	}

	patchedSha1 = getOriginalSHA1(); // initially it's the same ..

	if (size != 0) {
		const XMLElement* patchesElem = config.findChild("patches");
		if (patchesElem) {
			auto_ptr<const PatchInterface> patch(
				new EmptyPatch(rom, size));

			FileContext& context = config.getFileContext();
			XMLElement::Children patches;
			patchesElem->getChildren("ips", patches);
			for (XMLElement::Children::const_iterator it
			       = patches.begin(); it != patches.end(); ++it) {
				Filename filename((*it)->getData(), context, controller);
				patch.reset(new IPSPatch(filename, patch));
			}
			unsigned patchSize = patch->getSize();
			if (patchSize <= size) {
				patch->copyBlock(0, const_cast<byte*>(rom), size);
			} else {
				size = patchSize;
				extendedRom.resize(size);
				patch->copyBlock(0, &extendedRom[0], size);
				rom = &extendedRom[0];
			}

			// .. but recalculate when there were patches
			patchedSha1 = SHA1::calc(rom, size);
		}
	}
	info = RomDatabase::instance().fetchRomInfo(cliComm, *this);

	// TODO fix this, this is a hack that depends heavily on MSXRomCLI.cc
	if (!info->getTitle().empty() && StringOp::startsWith(name, "MSXRom")) {
		name = info->getTitle();
	}

	if (size) {
		Debugger& debugger = motherBoard.getDebugger();
		if (debugger.findDebuggable(name)) {
			unsigned n = 0;
			string tmp;
			do {
				tmp = name + " (" + StringOp::toString(++n) + ")";
			} while (debugger.findDebuggable(tmp));
			name = tmp;
		}
		romDebuggable.reset(new RomDebuggable(debugger, *this));
	}

	XMLElement& mutableConfig = const_cast<XMLElement&>(config);
	const XMLElement& actualSha1Elem = mutableConfig.getCreateChild(
		"resolvedSha1", patchedSha1);
	if (actualSha1Elem.getData() != patchedSha1) {
		string tmp = file.get() ? file->getURL() : name;
		// can only happen in case of loadstate
		motherBoard.getMSXCliComm().printWarning(
			"The content of the rom " + tmp +
			" has changed since the time this savestate was "
			"created. This might result in emulation problems.");
	}
}



void Rom::read(const XMLElement& config)
{
	assert(file.get());

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
		throw MSXException("Offset greater than filesize");
	}
	size = fileSize - offset;

	// read file
	byte* tmp = 0; // avoid warning
	try {
		tmp = file->mmap() + offset;
		rom = tmp;
	} catch (FileException&) {
		throw MSXException("Error reading ROM image: " +
		                   file->getURL());
	}

	// verify SHA1
	if (!checkSHA1(config)) {
		motherBoard.getMSXCliComm().printWarning(
			"SHA1 sum for '" + config.getId() +
			"' does not match with sum of '" +
			file->getURL() + "'.");
	}
}

bool Rom::checkSHA1(const XMLElement& config)
{
	const string& sha1sum = getOriginalSHA1();
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
}

const RomInfo& Rom::getInfo() const
{
	return *info;
}

const string& Rom::getName() const
{
	return name;
}

const string& Rom::getDescription() const
{
	return description;
}

const string& Rom::getOriginalSHA1() const
{
	if (originalSha1.empty()) {
		assert(patchedSha1.empty());
		originalSha1 = SHA1::calc(rom, size);
	}
	return originalSha1;
}
const string& Rom::getPatchedSHA1() const
{
	assert(!originalSha1.empty());
	assert(!patchedSha1.empty());
	return patchedSha1;
}


RomDebuggable::RomDebuggable(Debugger& debugger_, Rom& rom_)
	: debugger(debugger_), rom(rom_)
{
	debugger.registerDebuggable(rom.getName(), *this);
}

RomDebuggable::~RomDebuggable()
{
	debugger.unregisterDebuggable(rom.getName(), *this);
}

unsigned RomDebuggable::getSize() const
{
	return rom.getSize();
}

const string& RomDebuggable::getDescription() const
{
	return rom.getDescription();
}

byte RomDebuggable::read(unsigned address)
{
	assert(address < getSize());
	return rom[address];
}

void RomDebuggable::write(unsigned /*address*/, byte /*value*/)
{
	// ignore
}

} // namespace openmsx
