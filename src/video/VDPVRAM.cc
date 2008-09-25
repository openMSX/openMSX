// $Id$

#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "Renderer.hh"
#include "Math.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include <cstring>

namespace openmsx {

// class VRAMWindow

DummyVRAMOBserver VRAMWindow::dummyObserver;

VRAMWindow::VRAMWindow(Ram& vram)
	: data(&vram[0])
	, sizeMask(Math::powerOfTwo(vram.getSize()) - 1)
{
	observer = &dummyObserver;
	baseAddr  = -1; // disable window
	baseMask = 0;
	indexMask = 0; // these 3 don't matter but it makes valgrind happy
	combiMask = 0;
}


// class LogicalVRAMDebuggable

class LogicalVRAMDebuggable : public SimpleDebuggable
{
public:
	explicit LogicalVRAMDebuggable(VDP& vdp);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value, const EmuTime& time);
private:
	unsigned transform(unsigned address);
	VDP& vdp;
	VDPVRAM& vram;
};


// TODO shouldn't CPU view always be 192kB? (or even 256kB)
LogicalVRAMDebuggable::LogicalVRAMDebuggable(VDP& vdp_)
        : SimpleDebuggable(vdp_.getMotherBoard(), "VRAM",
	                   "CPU view on video RAM given the current display mode.",
	                   vdp_.getVRAM().getSize())
	, vdp(vdp_)
	, vram(vdp.getVRAM())
{
}

unsigned LogicalVRAMDebuggable::transform(unsigned address)
{
	// TODO not correct wrt interleaving of extended vram
	return vdp.getDisplayMode().isPlanar()
	     ? ((address << 16) | (address >> 1)) & 0x1FFFF
	     : address;
}

byte LogicalVRAMDebuggable::read(unsigned address, const EmuTime& time)
{
	return vram.cpuRead(transform(address), time);
}

void LogicalVRAMDebuggable::write(unsigned address, byte value, const EmuTime& time)
{
	vram.cpuWrite(transform(address), value, time);
}


// class VDPVRAM

static unsigned bufferSize(unsigned size)
{
	// for 16kb vram still allocate a 32kb buffer
	//  (mirroring happens at 32kb, upper 16kb contains random data)
	return (size != 0x4000) ? size : 0x8000;
}

VDPVRAM::VDPVRAM(VDP& vdp_, unsigned size, const EmuTime& time)
	: vdp(vdp_)
	, data(vdp.getMotherBoard(), "physical VRAM",
	       "VDP-screen-mode-independend view on the video RAM.", bufferSize(size))
	, logicalVRAMDebug(new LogicalVRAMDebuggable(vdp))
	#ifdef DEBUG
	, vramTime(time)
	#endif
	, sizeMask(Math::powerOfTwo(data.getSize()) - 1)
	, actualSize(size)
	, cmdReadWindow(data)
	, cmdWriteWindow(data)
	, nameTable(data)
	, colourTable(data)
	, patternTable(data)
	, bitmapVisibleWindow(data)
	, bitmapCacheWindow(data)
	, spriteAttribTable(data)
	, spritePatternTable(data)
{
	(void)time;

	// Initialise VRAM data array.
	// TODO: Fill with checkerboard pattern NMS8250 has.
	memset(&data[0], 0, data.getSize());
	if (actualSize == 0x4000) {
		// [0x4000,0x8000) contains random data
		// TODO reading same location multiple times does not always
		// give the same value
		memset(&data[0x4000], 0xFF, 0x4000);
	}

	// Whole VRAM is cachable.
	// Because this window has no observer, any EmuTime can be passed.
	// TODO: Move this to cache registration.
	bitmapCacheWindow.setMask(0x1FFFF, -1 << 17, EmuTime::zero);
	
}

VDPVRAM::~VDPVRAM()
{
}

void VDPVRAM::updateDisplayMode(DisplayMode mode, const EmuTime& time)
{
	renderer->updateDisplayMode(mode, time);
	cmdEngine->updateDisplayMode(mode, time);
	spriteChecker->updateDisplayMode(mode, time);
}

void VDPVRAM::updateDisplayEnabled(bool enabled, const EmuTime& time)
{
	assert(vdp.isInsideFrame(time));
	renderer->updateDisplayEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateDisplayEnabled(enabled, time);
}

void VDPVRAM::updateSpritesEnabled(bool enabled, const EmuTime& time)
{
	assert(vdp.isInsideFrame(time));
	renderer->updateSpritesEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateSpritesEnabled(enabled, time);
}

void VDPVRAM::setRenderer(Renderer* renderer, const EmuTime& time)
{
	this->renderer = renderer;

	bitmapVisibleWindow.resetObserver();
	// Set up bitmapVisibleWindow to full VRAM.
	// TODO: Have VDP/Renderer set the actual range.
	bitmapVisibleWindow.setMask(0x1FFFF, -1 << 17, time);
	// TODO: If it is a good idea to send an initial sync,
	//       then call setObserver before setMask.
	bitmapVisibleWindow.setObserver(renderer);
}

void VDPVRAM::change4k8kMapping(bool mapping8k)
{
	/* Sources:
	 *  - http://www.msx.org/forumtopicl8624.html
	 *  - Charles MacDonald's sc3000h.txt (http://cgfm2.emuviews.com)
	 *
	 * Bit 7 of register #1 affects how the VDP generates addresses when
	 * accessing VRAM. Here's a table illustrating the differences:
	 *
	 * VDP address      VRAM address
	 * (Column)         4K mode     8/16K mode
	 * AD0              VA0         VA0
	 * AD1              VA1         VA1
	 * AD2              VA2         VA2
	 * AD3              VA3         VA3
	 * AD4              VA4         VA4
	 * AD5              VA5         VA5
	 * AD6              VA12        VA6
	 * AD7              Not used    Not used
	 * (Row)
	 * AD0              VA6         VA7
	 * AD1              VA7         VA8
	 * AD2              VA8         VA9
	 * AD3              VA9         VA10
	 * AD4              VA10        VA11
	 * AD5              VA11        VA12
	 * AD6              VA13        VA13
	 * AD7              Not used    Not used
	 *
	 * ADx - TMS9928 8-bit VRAM address/data bus
	 * VAx - 14-bit VRAM address that the VDP wants to access
	 *
	 * How the address is formed has to do with the physical layout of
	 * memory cells in a DRAM chip. A 4Kx1 chip has 64x64 cells, a 8Kx1 or
	 * 16Kx1 chip has 128x64 or 128x128 cells. Because the DRAM address bus
	 * is multiplexed, this means 6 bits are used for 4K DRAMs and 7 bits
	 * are used for 8K or 16K DRAMs.
	 *
	 * In 4K mode the 6 bits of the row and column are output first, with
	 * the remaining high-order bits mapped to AD6. In 8/16K mode the 7
	 * bits of the row and column are output normally. This also means that
	 * even in 4K mode, all 16K of VRAM can be accessed. The only
	 * difference is in what addresses are used to store data.
	 */
	byte tmp[0x4000];
	if (mapping8k) {
		// from 8k/16k to 4k mapping
		for (unsigned addr8 = 0; addr8 < 0x4000; addr8 += 64) {
			unsigned addr4 =  (addr8 & 0x203F) |
			                 ((addr8 & 0x1000) >> 6) |
			                 ((addr8 & 0x0FC0) << 1);
			const byte* src = &data[addr8];
			byte* dst = &tmp[addr4];
			memcpy(dst, src, 64);
		}
	} else {
		// from 4k to 8k/16k mapping
		for (unsigned addr4 = 0; addr4 < 0x4000; addr4 += 64) {
			unsigned addr8 =  (addr4 & 0x203F) |
			                 ((addr4 & 0x0040) << 6) |
			                 ((addr4 & 0x1F80) >> 1);
			const byte* src = &data[addr4];
			byte* dst = &tmp[addr8];
			memcpy(dst, src, 64);
		}
	}
	memcpy(&data[0], tmp, 0x4000);
}


template<typename Archive>
void VRAMWindow::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("baseAddr",  baseAddr);
	ar.serialize("baseMask",  baseMask);
	ar.serialize("indexMask", indexMask);
	if (ar.isLoader()) {
		combiMask = ~baseMask | indexMask;
		// TODO ?  observer->updateWindow(isEnabled(), time);
	}
}

template<typename Archive>
void VDPVRAM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("data", &data[0], actualSize);
	ar.serialize("cmdReadWindow",       cmdReadWindow);
	ar.serialize("cmdWriteWindow",      cmdWriteWindow);
	ar.serialize("nameTable",           nameTable);
	ar.serialize("colourTable",         colourTable);
	ar.serialize("patternTable",        patternTable);
	ar.serialize("bitmapVisibleWindow", bitmapVisibleWindow);
	ar.serialize("bitmapCacheWindow",   bitmapCacheWindow);
	ar.serialize("spriteAttribTable",   spriteAttribTable);
	ar.serialize("spritePatternTable",  spritePatternTable);
}
INSTANTIATE_SERIALIZE_METHODS(VDPVRAM);

} // namespace openmsx
