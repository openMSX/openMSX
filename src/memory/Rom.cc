// $Id$

#include "Rom.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "RomInfo.hh"
#include "RomDatabase.hh"
#include "File.hh"
#include "FileContext.hh"
#include "Filename.hh"
#include "FileException.hh"
#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "Debugger.hh"
#include "Debuggable.hh"
#include "CliComm.hh"
#include "FilePool.hh"
#include "ConfigException.hh"
#include "EmptyPatch.hh"
#include "IPSPatch.hh"
#include "StringOp.hh"
#include "sha1.hh"
#include "memory.hh"
#include <limits>
#include <sstream>
#include <cstring>

using std::string;
using std::unique_ptr;

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


Rom::Rom(const string& name_, const string& description_,
         const DeviceConfig& config, const string& id /*= ""*/)
	: name(name_), description(description_)
{
	// Try all <rom> tags with matching "id" attribute.
	string errors;
	for (auto& c : config.getXML()->getChildren("rom")) {
		if (c->getAttribute("id", "") == id) {
			try {
				init(config.getMotherBoard(), *c, config.getFileContext());
				return;
			} catch (MSXException& e) {
				// remember error message, and try next
				if (!errors.empty() && (errors.back() != '\n')) {
					errors += '\n';
				}
				errors += e.getMessage();
			}
		}
	}
	if (errors.empty()) {
		// No matching <rom> tag.
		StringOp::Builder err;
		err << "Missing <rom> tag";
		if (!id.empty()) {
			err << " with id=\"" << id << '"';
		}
		throw ConfigException(err);
	} else {
		// We got at least one matching <rom>, but it failed to load.
		// Report error messages of all failed attempts.
		throw ConfigException(errors);
	}
}

void Rom::init(MSXMotherBoard& motherBoard, const XMLElement& config,
               const FileContext& context)
{
	// (Only) if the content of this ROM depends on state that is not part
	// of a savestate, we want to compare the sha1sum of the ROM from the
	// time the savestate was created with the one from the loaded
	// savestate. External state can be a .rom file or a patch file.
	bool checkResolvedSha1 = false;

	auto sums = config.getChildren("sha1");
	const XMLElement* resolvedFilenameElem = config.findChild("resolvedFilename");
	const XMLElement* resolvedSha1Elem     = config.findChild("resolvedSha1");
	const XMLElement* filenameElem         = config.findChild("filename");
	if (config.findChild("firstblock")) {
		// part of the TurboR main ROM
		//  If there is a firstblock/lastblock tag, (only) use these to
		//  locate the rom. In the past we would also write add a
		//  resolvedSha1 tag for this type of ROM (before we used
		//  'checkResolvedSha1'). So there are old savestates that have
		//  both type of tags. For such a savestate it's important to
		//  first check firstblock, otherwise it will only load when
		//  there is a file that matches the sha1sums of the
		//  firstblock-lastblock portion of the containing file.
		int first = config.getChildDataAsInt("firstblock");
		int last  = config.getChildDataAsInt("lastblock");
		size = (last - first + 1) * 0x2000;
		rom = motherBoard.getPanasonicMemory().getRomRange(first, last);
		assert(rom);

		// Part of a bigger (already checked) rom, no need to check.
		checkResolvedSha1 = false;

	} else if (resolvedFilenameElem || resolvedSha1Elem ||
	    !sums.empty() || filenameElem) {
		FilePool& filepool = motherBoard.getReactor().getFilePool();
		// first try already resolved filename ..
		if (resolvedFilenameElem) {
			try {
				file = make_unique<File>(
					resolvedFilenameElem->getData());
			} catch (FileException&) {
				// ignore
			}
		}
		// .. then try the actual sha1sum ..
		FilePool::FileType fileType = context.isUserContext()
			? FilePool::ROM : FilePool::SYSTEM_ROM;
		if (!file.get() && resolvedSha1Elem) {
			Sha1Sum sha1(resolvedSha1Elem->getData());
			file = filepool.getFile(fileType, sha1);
			if (file.get()) {
				// avoid recalculating same sha1 later
				originalSha1 = sha1;
			}
		}
		// .. and then try filename as originally given by user ..
		if (!file.get() && filenameElem) {
			string name = filenameElem->getData();
			try {
				Filename filename(name, context);
				file = make_unique<File>(filename);
			} catch (FileException&) {
				// ignore
			}
		}
		// .. then try all alternative sha1sums ..
		// (this might retry the actual sha1sum)
		if (!file.get()) {
			for (auto& s : sums) {
				Sha1Sum sha1(s->getData());
				file = filepool.getFile(fileType, sha1);
				if (file.get()) {
					// avoid recalculating same sha1 later
					originalSha1 = sha1;
					break;
				}
			}
		}
		// .. still no file, then error
		if (!file.get()) {
			StringOp::Builder error;
			error << "Couldn't find ROM file for \""
			      << name << '"';
			if (filenameElem) {
				error << ' ' << filenameElem->getData();
			}
			if (resolvedSha1Elem) {
				error << " (" << resolvedSha1Elem->getData() << ')';
			}
			error << '.';
			throw MSXException(error);
		}

		// actually read file content
		if (config.findChild("filesize") ||
		    config.findChild("skip_headerbytes")) {
			throw MSXException(
				"The <filesize> and <skip_headerbytes> tags "
				"inside a <rom> section are no longer "
				"supported.");
		}
		try {
			size_t size2;
			rom = file->mmap(size2);
			if (size2 > std::numeric_limits<decltype(size)>::max()) {
				throw MSXException("Rom file too big: " +
				                   file->getURL());
			}
			size = size2;
		} catch (FileException&) {
			throw MSXException("Error reading ROM image: " +
					   file->getURL());
		}

		// For file-based roms, calc sha1 via File::getSha1Sum(). It can
		// possibly use the FilePool cache to avoid the calculation.
		if (originalSha1.empty()) {
			file->setFilePool(filepool);
			originalSha1 = file->getSha1Sum();
		}

		// verify SHA1
		if (!checkSHA1(config)) {
			motherBoard.getMSXCliComm().printWarning(
				StringOp::Builder() <<
				"SHA1 sum for '" << name <<
				"' does not match with sum of '" <<
				file->getURL() << "'.");
		}

		// We loaded an extrenal file, so check.
		checkResolvedSha1 = true;

	} else {
		// for an empty SCC the <size> tag is missing, so take 0
		// for MegaFlashRomSCC the <size> tag is used to specify
		// the size of the mapper (and you don't care about initial
		// content)
		size = config.getChildDataAsInt("size", 0) * 1024; // in kb
		extendedRom.resize(size);
		memset(extendedRom.data(), 0xff, size);
		rom = extendedRom.data();

		// Content does not depend on external files. No need to check
		checkResolvedSha1 = false;
	}

	if (size != 0) {
		if (auto* patchesElem = config.findChild("patches")) {
			// calculate before content is altered
			getOriginalSHA1();

			unique_ptr<PatchInterface> patch =
				make_unique<EmptyPatch>(rom, size);

			for (auto& p : patchesElem->getChildren("ips")) {
				Filename filename(p->getData(), context);
				patch = make_unique<IPSPatch>(
					filename, std::move(patch));
			}
			unsigned patchSize = patch->getSize();
			if (patchSize <= size) {
				patch->copyBlock(0, const_cast<byte*>(rom), size);
			} else {
				size = patchSize;
				extendedRom.resize(size);
				patch->copyBlock(0, extendedRom.data(), size);
				rom = extendedRom.data();
			}

			// calculated because it's different from original
			patchedSha1 = SHA1::calc(rom, size);

			// Content altered by external patch file -> check.
			checkResolvedSha1 = true;
		}
	}

	// TODO fix this, this is a hack that depends heavily on
	//      HardwareConig::createRomConfig
	if (StringOp::startsWith(name, "MSXRom")) {
		const RomInfo* romInfo = motherBoard.getReactor().
			getSoftwareDatabase().fetchRomInfo(getOriginalSHA1());
		if ((romInfo != nullptr) && !romInfo->getTitle().empty()) {
			name = romInfo->getTitle();
		} else {
			// unknown ROM, use file name
			name = file->getOriginalName();
		}
	}

	if (size) {
		Debugger& debugger = motherBoard.getDebugger();
		if (debugger.findDebuggable(name)) {
			unsigned n = 0;
			string tmp;
			do {
				tmp = StringOp::Builder() << name << " (" << ++n << ')';
			} while (debugger.findDebuggable(tmp));
			name = tmp;
		}
		romDebuggable = make_unique<RomDebuggable>(debugger, *this);
	}

	if (checkResolvedSha1) {
		auto& mutableConfig = const_cast<XMLElement&>(config);
		string patchedSha1Str = getPatchedSHA1().toString();
		const XMLElement& actualSha1Elem = mutableConfig.getCreateChild(
			"resolvedSha1", patchedSha1Str);
		if (actualSha1Elem.getData() != patchedSha1Str) {
			string tmp = file.get() ? file->getURL() : name;
			// can only happen in case of loadstate
			motherBoard.getMSXCliComm().printWarning(
				"The content of the rom " + tmp + " has "
				"changed since the time this savestate was "
				"created. This might result in emulation "
				"problems.");
		}
	}

	// This must come after we store the 'resolvedSha1', because on
	// loadstate we use that tag to search the complete rom in a filepool.
	if (const XMLElement* windowElem = config.findChild("window")) {
		unsigned windowBase = windowElem->getAttributeAsInt("base", 0);
		unsigned windowSize = windowElem->getAttributeAsInt("size", size);
		if ((windowBase + windowSize) > size) {
			throw MSXException(StringOp::Builder() <<
				"The specified window [" << windowBase << ',' <<
				windowBase + windowSize << ") falls outside "
				"the rom (with size " << size << ").");
		}
		rom = &rom[windowBase];
		size = windowSize;
		patchedSha1 = SHA1::calc(rom, size);
	}
}

bool Rom::checkSHA1(const XMLElement& config)
{
	auto sums = config.getChildren("sha1");
	if (sums.empty()) {
		return true;
	}
	string sha1sum = getOriginalSHA1().toString();
	for (auto& s : sums) {
		if (s->getData() == sha1sum) {
			return true;
		}
	}
	return false;
}

Rom::~Rom()
{
}

string Rom::getFilename() const
{
	return file.get() ? file->getURL() : "";
}

const string& Rom::getName() const
{
	return name;
}

const string& Rom::getDescription() const
{
	return description;
}

const Sha1Sum& Rom::getOriginalSHA1() const
{
	if (originalSha1.empty()) {
		assert(patchedSha1.empty());
		originalSha1 = SHA1::calc(rom, size);
	}
	return originalSha1;
}
const Sha1Sum& Rom::getPatchedSHA1() const
{
	return patchedSha1.empty() ? getOriginalSHA1() : patchedSha1;
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
