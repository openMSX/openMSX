// $Id$

/*
 * Adriano Camargo Rodrigues da Cunha wrote:
 *
 *  Any address inside a 8k page can change the page.  In other
 *  words:
 *
 *  for 4000h-5FFFh, mapping addresses are 4000h-5FFFh
 *  for 6000h-7FFFh, mapping addresses are 6000h-7FFFh
 *  for 8000h-9FFFh, mapping addresses are 8000h-9FFFh
 *  for A000h-BFFFh, mapping addresses are A000h-BFFFh
 *
 *  If you do an IN A,(8Eh) (the value of A register is unknown and
 *  never used) you can write on MegaRAM pages, but you can't map
 *  pages. If you do an OUT (8Eh),A (the value of A register doesn't
 *  matter) you can't write to MegaRAM pages, but you can map them.
 *
 *  Another thing: the MegaRAMs of Ademir Carchano have a mirror
 *  effect: if you map the page 0 of MegaRAM slot, you'll be
 *  acessing the same area of 8000h-BFFFh of this slot; if you map
 *  the page 3 of MegaRAM slot, you'll be acessing the same area of
 *  4000h-7FFFh of this slot. I don't know any software that makes
 *  use of this feature, except UZIX for MSX1.
 */

#include "MSXMegaRam.hh"
#include "XMLElement.hh"
#include "MSXCPU.hh"
#include "Ram.hh"
#include "Rom.hh"


namespace openmsx {

static int roundUpPow2(int a)
{
	int result = 1;
	while (a > result) {
		result <<= 1;
	}
	return result;
}

MSXMegaRam::MSXMegaRam(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	int size = config.getChildDataAsInt("size");
	numBlocks = size / 8;	// 8kb blocks
	maskBlocks = roundUpPow2(numBlocks) - 1;
	ram.reset(new Ram(getName() + " RAM", "Mega-RAM", numBlocks * 0x2000));
	if (config.findChild("rom")) {
		rom.reset(new Rom(getName() + " ROM", "Mega-RAM DiskROM", config));
	}
	
	for (int i = 0; i < 4; i++) {
		setBank(i, 0);
	}
	writeMode = false;
	reset(time);
}

MSXMegaRam::~MSXMegaRam()
{
}

void MSXMegaRam::reset(const EmuTime& /*time*/)
{
	// selected banks nor writeMode does change after reset
	romMode = rom.get(); // select rom mode if there is a rom
}

byte MSXMegaRam::readMem(word address, const EmuTime& /*time*/)
{
	return *getReadCacheLine(address);
}

const byte* MSXMegaRam::getReadCacheLine(word address) const
{
	if (romMode) {
		// gcc-3.3 produces wrong code for this line !!
		// if (address >= 0x4000 && address <= 0xbfff) {
		if (!((address - 0x4000) & 0x8000)) {
			return &(*rom)[address - 0x4000];
		}
		return unmappedRead;
	}
	int block = bank[(address & 0x7FFF) / 0x2000];
	return (block < numBlocks)
	     ? &(*ram)[(block * 0x2000) + (address & 0x1FFF)]
	     : unmappedRead;
}

void MSXMegaRam::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	byte* tmp = getWriteCacheLine(address);
	if (tmp) {
		*tmp = value;
	} else {
		assert(!romMode && !writeMode);
		setBank((address & 0x7FFF) / 0x2000, value);
	}
}

byte* MSXMegaRam::getWriteCacheLine(word address) const
{
	if (romMode) return unmappedWrite;
	if (writeMode) {
		int block = bank[(address & 0x7FFF) / 0x2000];
		return (block < numBlocks)
		     ? &(*ram)[(block * 0x2000) + (address & 0x1FFF)]
		     : unmappedWrite;
	} else {
		return NULL;
	}
}

byte MSXMegaRam::readIO(byte port, const EmuTime& /*time*/)
{
	switch (port) {
		case 0x8E:
			// enable writing
			writeMode = true;
			romMode = false;
			break;
		case 0x8F:
			if (rom.get()) romMode = true;
			break;
	}
	MSXCPU::instance().invalidateMemCache(0x0000, 0x10000);
	return 0xFF;	// return value doesn't matter
}

byte MSXMegaRam::peekIO(byte /*port*/, const EmuTime& /*time*/) const
{
	return 0xFF;
}

void MSXMegaRam::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	switch (port) {
		case 0x8E:
			// enable switching
			writeMode = false;
			romMode = false;
			break;
		case 0x8F:
			if (rom.get()) romMode = true;
			break;
	}
	MSXCPU::instance().invalidateMemCache(0x0000, 0x10000);
}

void MSXMegaRam::setBank(byte page, byte block)
{
	bank[page] = block & maskBlocks;
	MSXCPU& cpu = MSXCPU::instance();
	word adr = page * 0x2000;
	cpu.invalidateMemCache(adr + 0x0000, 0x2000);
	cpu.invalidateMemCache(adr + 0x8000, 0x2000);
}

} // namespace openmsx
