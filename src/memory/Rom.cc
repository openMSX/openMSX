#include "Rom.hh"

#include "ConfigException.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "DeviceConfig.hh"
#include "EmptyPatch.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FilePool.hh"
#include "Filename.hh"
#include "HardwareConfig.hh"
#include "IPSPatch.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "MemBuffer.hh"
#include "PanasonicMemory.hh"
#include "Reactor.hh"
#include "RomDatabase.hh"
#include "RomInfo.hh"
#include "XMLElement.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "sha1.hh"
#include "stl.hh"

#include <memory>

using std::string;

namespace openmsx {

class RomDebuggable final : public Debuggable
{
public:
	RomDebuggable(Debugger& debugger, Rom& rom);
	RomDebuggable(const RomDebuggable&) = delete;
	RomDebuggable(RomDebuggable&&) = delete;
	RomDebuggable& operator=(const RomDebuggable&) = delete;
	RomDebuggable& operator=(RomDebuggable&&) = delete;
	~RomDebuggable();

	[[nodiscard]] unsigned getSize() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] byte read(unsigned address) override;
	void write(unsigned address, byte value) override;
	void moved(Rom& r);
private:
	Debugger& debugger;
	Rom* rom;
};


Rom::Rom(string name_, static_string_view description_,
         const DeviceConfig& config, const string& id /*= {}*/)
	: name(std::move(name_)), description(description_)
{
	// Try all <rom> tags with matching "id" attribute.
	string errors;
	for (const auto* c : config.getXML()->getChildren("rom")) {
		if (c->getAttributeValue("id", {}) == id) {
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
		string err = "Missing <rom> tag";
		if (!id.empty()) {
			strAppend(err, " with id=\"", id, '"');
		}
		throw ConfigException(std::move(err));
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

	auto sums      = to_vector(config.getChildren("sha1"));
	auto filenames = to_vector(config.getChildren("filename"));
	const auto* resolvedFilenameElem = config.findChild("resolvedFilename");
	const auto* resolvedSha1Elem     = config.findChild("resolvedSha1");
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
		unsigned first = config.getChildDataAsInt("firstblock", 0);
		unsigned last  = config.getChildDataAsInt("lastblock", 0);
		rom = motherBoard.getPanasonicMemory().getRomRange(first, last);
		assert(rom.data());

		// Part of a bigger (already checked) rom, no need to check.
		checkResolvedSha1 = false;

	} else if (resolvedFilenameElem || resolvedSha1Elem ||
	           !sums.empty() || !filenames.empty()) {
		auto& filePool = motherBoard.getReactor().getFilePool();
		// first try already resolved filename ..
		if (resolvedFilenameElem) {
			try {
				file = File(std::string(resolvedFilenameElem->getData()));
			} catch (FileException&) {
				// ignore
			}
		}
		// .. then try the actual sha1sum ..
		auto fileType = context.isUserContext()
			? FileType::ROM : FileType::SYSTEM_ROM;
		if (!file.is_open() && resolvedSha1Elem) {
			Sha1Sum sha1(resolvedSha1Elem->getData());
			file = filePool.getFile(fileType, sha1);
			if (file.is_open()) {
				// avoid recalculating same sha1 later
				originalSha1 = sha1;
			}
		}
		// .. and then try filename as originally given by user ..
		if (!file.is_open()) {
			for (auto& f : filenames) {
				try {
					file = File(Filename(f->getData(), context));
					break;
				} catch (FileException&) {
					// ignore
				}
			}
		}
		// .. then try all alternative sha1sums ..
		// (this might retry the actual sha1sum)
		if (!file.is_open()) {
			for (auto& s : sums) {
				Sha1Sum sha1(s->getData());
				file = filePool.getFile(fileType, sha1);
				if (file.is_open()) {
					// avoid recalculating same sha1 later
					originalSha1 = sha1;
					break;
				}
			}
		}
		// .. still no file, then error
		if (!file.is_open()) {
			string error = strCat("Couldn't find ROM file for \"", name, '"');
			if (!filenames.empty()) {
				strAppend(error, ' ', filenames.front()->getData());
			}
			if (resolvedSha1Elem) {
				strAppend(error, " (sha1: ", resolvedSha1Elem->getData(), ')');
			} else if (!sums.empty()) {
				strAppend(error, " (sha1: ", sums.front()->getData(), ')');
			}
			strAppend(error, '.');
			throw MSXException(std::move(error));
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
			auto mmap = file.mmap();
			rom = mmap;
		} catch (FileException&) {
			throw MSXException("Error reading ROM image: ", file.getURL());
		}

		// For file-based roms, calc sha1 via File::getSha1Sum(). It can
		// possibly use the FilePool cache to avoid the calculation.
		if (originalSha1.empty()) {
			originalSha1 = filePool.getSha1Sum(file);
		}

		// verify SHA1
		if (!checkSHA1(config)) {
			motherBoard.getMSXCliComm().printWarning(
				"SHA1 sum for '", name,
				"' does not match with sum of '",
				file.getURL(), "'.");
		}

		// We loaded an external file, so check.
		checkResolvedSha1 = true;

	} else {
		// for an empty SCC the <size> tag is missing, so take 0
		// for MegaFlashRomSCC the <size> tag is used to specify
		// the size of the mapper (and you don't care about initial
		// content)
		unsigned size = config.getChildDataAsInt("size", 0) * 1024; // in kb
		extendedRom.resize(size);
		ranges::fill(std::span{extendedRom}, 0xff);
		rom = std::span{extendedRom};

		// Content does not depend on external files. No need to check
		checkResolvedSha1 = false;
	}

	if (!rom.empty()) {
		if (const auto* patchesElem = config.findChild("patches")) {
			// calculate before content is altered
			(void)getOriginalSHA1(); // fills cache

			std::unique_ptr<PatchInterface> patch =
				std::make_unique<EmptyPatch>(rom);

			for (const auto* p : patchesElem->getChildren("ips")) {
				patch = std::make_unique<IPSPatch>(
					Filename(p->getData(), context),
					std::move(patch));
			}
			auto patchSize = patch->getSize();
			if (patchSize <= rom.size()) {
				patch->copyBlock(0, std::span{const_cast<uint8_t*>(rom.data()), rom.size()});
			} else {
				MemBuffer<byte> extendedRom2(patchSize);
				patch->copyBlock(0, std::span{extendedRom2});
				extendedRom = std::move(extendedRom2);
				rom = std::span{extendedRom};
			}

			// calculated because it's different from original
			actualSha1 = SHA1::calc(rom);

			// Content altered by external patch file -> check.
			checkResolvedSha1 = true;
		}
	}

	// TODO fix this, this is a hack that depends heavily on
	//      HardwareConfig::createRomConfig
	if (name.starts_with("MSXRom")) {
		const auto& db = motherBoard.getReactor().getSoftwareDatabase();
		std::string_view title;
		if (const auto* romInfo = db.fetchRomInfo(getOriginalSHA1())) {
			title = romInfo->getTitle(db.getBufferStart());
		}
		if (!title.empty()) {
			name = title;
		} else {
			// unknown ROM, use file name
			name = file.getOriginalName();
		}
	}

	// Make name unique wrt all registered debuggables.
	auto& debugger = motherBoard.getDebugger();
	if (!rom.empty() && debugger.findDebuggable(name)) {
		unsigned n = 0;
		string tmp;
		do {
			tmp = strCat(name, " (", ++n, ')');
		} while (debugger.findDebuggable(tmp));
		name = std::move(tmp);
	}

	if (checkResolvedSha1) {
		auto& doc = motherBoard.getMachineConfig()->getXMLDocument();
		auto patchedSha1Str = getSHA1().toString();
		const auto* actualSha1Elem = doc.getOrCreateChild(
			const_cast<XMLElement&>(config),
			"resolvedSha1", doc.allocateString(patchedSha1Str));
		if (actualSha1Elem->getData() != patchedSha1Str) {
			std::string_view tmp = file.is_open() ? file.getURL() : name;
			// can only happen in case of loadstate
			motherBoard.getMSXCliComm().printWarning(
				"The content of the rom ", tmp, " has "
				"changed since the time this savestate was "
				"created. This might result in emulation "
				"problems.");
		}
	}

	// This must come after we store the 'resolvedSha1', because on
	// loadstate we use that tag to search the complete rom in a filePool.
	if (const auto* windowElem = config.findChild("window")) {
		unsigned windowBase = windowElem->getAttributeValueAsInt("base", 0);
		unsigned windowSize = windowElem->getAttributeValueAsInt("size", narrow_cast<int>(rom.size()));
		if ((windowBase + windowSize) > rom.size()) {
			throw MSXException(
				"The specified window [", windowBase, ',',
				windowBase + windowSize, ") falls outside "
				"the rom (with size ", rom.size(), ").");
		}
		rom = rom.subspan(windowBase, windowSize);
	}

	// Only create the debuggable once all checks succeeded.
	if (!rom.empty()) {
		romDebuggable = std::make_unique<RomDebuggable>(debugger, *this);
	}
}

bool Rom::checkSHA1(const XMLElement& config) const
{
	auto sums = to_vector(config.getChildren("sha1"));
	return sums.empty() ||
	       contains(sums, getOriginalSHA1(),
	                [](const auto* s) { return Sha1Sum(s->getData()); });
}

Rom::Rom(Rom&& r) noexcept
	: rom          (r.rom)
	, extendedRom  (std::move(r.extendedRom))
	, file         (std::move(r.file))
	, originalSha1 (r.originalSha1)
	, actualSha1   (r.actualSha1)
	, name         (std::move(r.name))
	, description  (r.description)
	, romDebuggable(std::move(r.romDebuggable))
{
	if (romDebuggable) romDebuggable->moved(*this);
}

Rom::~Rom() = default;

std::string_view Rom::getFilename() const
{
	return file.is_open() ? file.getURL() : std::string_view();
}

const Sha1Sum& Rom::getOriginalSHA1() const
{
	if (originalSha1.empty()) {
		originalSha1 = SHA1::calc(rom);
	}
	return originalSha1;
}

const Sha1Sum& Rom::getSHA1() const
{
	if (actualSha1.empty())
	{
		actualSha1 = getOriginalSHA1();
	}
	return actualSha1;
}

void Rom::addPadding(size_t newSize, byte filler)
{
	assert(newSize >= rom.size());
	if (newSize == rom.size()) return;

	MemBuffer<byte> tmp(newSize);
	ranges::copy(rom, std::span{tmp});
	ranges::fill(tmp.subspan(rom.size()), filler);

	rom = std::span{tmp};
	extendedRom = std::move(tmp);
}

void Rom::getInfo(TclObject& result) const
{
	result.addDictKeyValues("actualSHA1", getSHA1().toString(),
	                        "originalSHA1", getOriginalSHA1().toString(),
	                        "filename", getFilename());
}


RomDebuggable::RomDebuggable(Debugger& debugger_, Rom& rom_)
	: debugger(debugger_), rom(&rom_)
{
	debugger.registerDebuggable(rom->getName(), *this);
}

RomDebuggable::~RomDebuggable()
{
	debugger.unregisterDebuggable(rom->getName(), *this);
}

unsigned RomDebuggable::getSize() const
{
	return narrow<unsigned>(rom->size());
}

std::string_view RomDebuggable::getDescription() const
{
	return rom->getDescription();
}

byte RomDebuggable::read(unsigned address)
{
	assert(address < getSize());
	return (*rom)[address];
}

void RomDebuggable::write(unsigned /*address*/, byte /*value*/)
{
	// ignore
}

void RomDebuggable::moved(Rom& r)
{
	rom = &r;
}

} // namespace openmsx
