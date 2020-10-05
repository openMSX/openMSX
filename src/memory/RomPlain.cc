#include "RomPlain.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

// return x inside [start, start+len)
static inline bool isInside(unsigned x, unsigned start, unsigned len)
{
	unsigned tmp = x - start;
	return tmp < len;
}

static std::string toString(unsigned start, unsigned len)
{
	return strCat("[0x", hex_string<4>(start), ", "
	               "0x", hex_string<4>(start + len), ')');
}

RomPlain::RomPlain(const DeviceConfig& config, Rom&& rom_,
                   MirrorType mirrored, int start)
	: Rom8kBBlocks(config, std::move(rom_))
{
	unsigned windowBase =  0x0000;
	unsigned windowSize = 0x10000;
	if (auto* mem = config.findChild("mem")) {
		windowBase = mem->getAttributeAsInt("base");
		windowSize = mem->getAttributeAsInt("size");
	}

	unsigned romSize = rom.getSize();
	if ((romSize > 0x10000) || (romSize & 0x1FFF)) {
		throw MSXException(rom.getName(),
			": invalid rom size: must be smaller than or equal to 64kB "
			"and must be a multiple of 8kB.");
	}

	unsigned romBase = (start == -1)
	                 ? guessLocation(windowBase, windowSize)
	                 : unsigned(start);
	if ((start == -1) &&
	    (!isInside(romBase,               windowBase, windowSize) ||
	     !isInside(romBase + romSize - 1, windowBase, windowSize))) {
		// ROM must fall inside the boundaries given by the <mem>
		// tag (this code only looks at one <mem> tag), but only
		// check when the start address was not explicitly specified
		throw MSXException(rom.getName(),
			": invalid rom position: interval ",
			toString(romBase, romSize), " must fit in ",
			toString(windowBase, windowSize), '.');
	}
	if ((romBase & 0x1FFF)) {
		throw MSXException(rom.getName(),
			": invalid rom position: must start at a 8kB boundary.");
	}

	unsigned firstPage = romBase / 0x2000;
	unsigned numPages = romSize / 0x2000;
	for (unsigned page = 0; page < 8; ++page) {
		unsigned romPage = page - firstPage;
		if (romPage < numPages) {
			setRom(page, romPage);
		} else {
			if (mirrored == MIRRORED) {
				setRom(page, romPage & (numPages - 1));
			} else {
				setUnmapped(page);
			}
		}
	}
	// RomPlain is currently implemented as a subclass of Rom8kBBlocks,
	// because of that it inherits a 8kB alignment requirement on the
	// base/size attributes in the <mem> tag in the hardware description.
	// But for example for the 'Boosted_audio' extension this requirement
	// is too strict. Except for the calls to setRom() and setUnmapped() in
	// this constructor, RomPlain doesn't dynamically change the memory
	// layout. So if we undo the cache-pre-filling stuff done by setRom()
	// it is OK to relax the alignment requirements again.
	//
	// An alternative is to not inherit from Rom8kBBlocks but from MSXRom.
	// That would simplify the alignment stuff, but OTOH then we have to
	// reimplement the debuggable stuff. Also the format of savestates will
	// change (so it would require extra backwards compatibility code).
	// Therefor, at least for now, I've not chosen this alternative.
	invalidateDeviceRCache();
}

void RomPlain::guessHelper(unsigned offset, int* pages)
{
	if ((rom[offset++] == 'A') && (rom[offset++] =='B')) {
		for (int i = 0; i < 4; i++) {
			word addr = rom[offset + 0] +
			            rom[offset + 1] * 256;
			offset += 2;
			if (addr) {
				int page = (addr >> 14) - (offset >> 14);
				if ((0 <= page) && (page <= 2)) {
					pages[page]++;
				}
			}
		}
	}
}

unsigned RomPlain::guessLocation(unsigned windowBase, unsigned windowSize)
{
	int pages[3] = { 0, 0, 0 };

	// count number of possible routine pointers
	if (rom.getSize() >= 0x0010) {
		guessHelper(0x0000, pages);
	}
	if (rom.getSize() >= 0x4010) {
		guessHelper(0x4000, pages);
	}

	// start address must be inside memory window
	if (!isInside(0x0000, windowBase, windowSize)) pages[0] = 0;
	if (!isInside(0x4000, windowBase, windowSize)) pages[1] = 0;
	if (!isInside(0x8000, windowBase, windowSize)) pages[2] = 0;

	// we prefer 0x4000, then 0x000 and then 0x8000
	if (pages[1] && (pages[1] >= pages[0]) && (pages[1] >= pages[2])) {
		return 0x4000;
	} else if (pages[0] && pages[0] >= pages[2]) {
		return 0x0000;
	} else if (pages[2]) {
		return 0x8000;
	}

	// heuristics didn't work, return start of window
	return windowBase;
}

unsigned RomPlain::getBaseSizeAlignment() const
{
	// Because this mapper has no switchable banks, we can relax the
	// alignment requirements again. See also the comment at the end of the
	// constructor.
	return MSXRom::getBaseSizeAlignment();
}

REGISTER_MSXDEVICE(RomPlain, "RomPlain");

} // namespace openmsx
