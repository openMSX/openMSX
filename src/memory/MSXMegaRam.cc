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
#include "Device.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


namespace openmsx {

MSXMegaRam::MSXMegaRam(Device* config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  MSXIODevice(config, time)
{
	int size = config->getParameterAsInt("size");
	int blocks = size / 8;	// 8kb blocks
	maxBlock = blocks - 1;
	ram = new byte[blocks * 0x2000];
	
	for (int i = 0; i < 4; i++) {
		setBank(i, 0);
	}
	writeMode = false;
}

MSXMegaRam::~MSXMegaRam()
{
	delete[] ram;
}

void MSXMegaRam::reset(const EmuTime& time)
{
	// selected banks nor writeMode does change after reset
}

byte MSXMegaRam::readMem(word address, const EmuTime& time)
{
	address &= 0x7FFF;
	byte page = address / 0x2000;
	byte result = ram[bank[page] * 0x2000 + (address & 0x1FFF)];
	return result;
}

const byte* MSXMegaRam::getReadCacheLine(word address) const
{
	address &= 0x7FFF;
	byte page = address / 0x2000;
	return &ram[bank[page] * 0x2000 + (address & 0x1FFF)];
}

void MSXMegaRam::writeMem(word address, byte value, const EmuTime& time)
{
	address &= 0x7FFF;
	byte page = address / 0x2000;
	if (writeMode) {
		ram[bank[page] * 0x2000 + (address & 0x1FFF)] = value;
	} else {
		setBank(page, value);
	}
}

byte* MSXMegaRam::getWriteCacheLine(word address) const
{
	address &= 0x7FFF;
	if (writeMode) {
		byte page = address / 0x2000;
		return &ram[bank[page] * 0x2000 + (address & 0x1FFF)];
	} else {
		return NULL;
	}
}

byte MSXMegaRam::readIO(byte port, const EmuTime& time)
{
	// enable writing
	writeMode = true;
	return 0xFF;	// return value doesn't matter
}

void MSXMegaRam::writeIO(byte port, byte value, const EmuTime& time)
{
	// enable switching
	writeMode = false;
	MSXCPU::instance()->invalidateCache(0x0000,
		0x10000 / CPU::CACHE_LINE_SIZE);
}

void MSXMegaRam::setBank(byte page, byte block)
{
	if (block > maxBlock) {
		block &= maxBlock;
	}
	bank[page] = block;
	MSXCPU* cpu = MSXCPU::instance();
	word adr = page * 0x2000;
	cpu->invalidateCache(adr,          0x2000 / CPU::CACHE_LINE_SIZE);
	cpu->invalidateCache(adr + 0x8000, 0x2000 / CPU::CACHE_LINE_SIZE);
}

} // namespace openmsx
