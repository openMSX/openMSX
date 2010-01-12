// $Id$

#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "Renderer.hh"
#include "Math.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include <algorithm>
#include <cstring>

namespace openmsx {

// class VRAMWindow

DummyVRAMOBserver VRAMWindow::dummyObserver;

VRAMWindow::VRAMWindow(Ram& vram)
	: data(&vram[0])
{
	observer = &dummyObserver;
	baseAddr  = -1; // disable window
	origBaseMask = 0;
	effectiveBaseMask = 0;
	indexMask = 0; // these 4 don't matter but it makes valgrind happy
	combiMask = 0;
	// sizeMask will be initialized shortly by the VDPVRAM class
}


// class LogicalVRAMDebuggable

/** This debuggable is always 128kB in size, even if the actual amount of VRAM
 *  is different:
 * - If there is less actual VRAM (e.g. 64kB), the upper regions of this
 *   debuggable act in the same way as if the CPU would read/write those
 *   addresses (mirroring or return fixed value / ignore write).
 * - If there is more VRAM (128kB + 64kB extended VRAM), the extended VRAM
 *   part is not accessible via this debuggable (it is via the 'physical VRAM'
 *   debuggable). We might change this in the future, but the interaction
 *   between interleaving and extended VRAM can be tricky, for example the
 *   size of this debuggable would have to be 256kB to be able to access the
 *   complete extended VRAM in interleaved mode.
 */
class LogicalVRAMDebuggable : public SimpleDebuggable
{
public:
	explicit LogicalVRAMDebuggable(VDP& vdp);
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	unsigned transform(unsigned address);
	VDP& vdp;
};


LogicalVRAMDebuggable::LogicalVRAMDebuggable(VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(), "VRAM",
	                   "CPU view on video RAM given the current display mode.",
	                   128 * 1024) // always 128kB
	, vdp(vdp_)
{
}

unsigned LogicalVRAMDebuggable::transform(unsigned address)
{
	return vdp.getDisplayMode().isPlanar()
	     ? ((address << 16) | (address >> 1)) & 0x1FFFF
	     : address;
}

byte LogicalVRAMDebuggable::read(unsigned address, EmuTime::param time)
{
	return vdp.getVRAM().cpuRead(transform(address), time);
}

void LogicalVRAMDebuggable::write(unsigned address, byte value, EmuTime::param time)
{
	vdp.getVRAM().cpuWrite(transform(address), value, time);
}


// class PhysicalVRAMDebuggable

class PhysicalVRAMDebuggable : public SimpleDebuggable
{
public:
	PhysicalVRAMDebuggable(VDP& vdp, VDPVRAM& vram, unsigned actualSize);
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	VDPVRAM& vram;
};


PhysicalVRAMDebuggable::PhysicalVRAMDebuggable(VDP& vdp, VDPVRAM& vram_,
                                               unsigned actualSize)
	: SimpleDebuggable(vdp.getMotherBoard(), "physical VRAM",
	                   "VDP-screen-mode-independent view on the video RAM.",
	                   actualSize)
	, vram(vram_) // vdp.getVRAM() doesn't work yet
{
}

byte PhysicalVRAMDebuggable::read(unsigned address, EmuTime::param time)
{
	return vram.cpuRead(address, time);
}

void PhysicalVRAMDebuggable::write(unsigned address, byte value, EmuTime::param time)
{
	vram.cpuWrite(address, value, time);
}


// class VDPVRAM

static unsigned bufferSize(unsigned size)
{
	// Always allocate at least a buffer of 128kB, this makes the VR0/VR1
	// swapping a lot easier. Actually only in case there is also extended
	// VRAM, we need to allocate more.
	// TODO possible optimization: for TMS99x8 we could allocate 16kB, it
	//      has no VR modes.
	return std::max(0x20000u, size);
}

VDPVRAM::VDPVRAM(VDP& vdp_, unsigned size, EmuTime::param time)
	: vdp(vdp_)
	, data(bufferSize(size))
	, logicalVRAMDebug (new LogicalVRAMDebuggable (vdp))
	, physicalVRAMDebug(new PhysicalVRAMDebuggable(vdp, *this, size))
	#ifdef DEBUG
	, vramTime(EmuTime::zero)
	#endif
	, actualSize(size)
	, cmdReadWindow(data)
	, cmdWriteWindow(data)
	, nameTable(data)
	, colorTable(data)
	, patternTable(data)
	, bitmapVisibleWindow(data)
	, bitmapCacheWindow(data)
	, spriteAttribTable(data)
	, spritePatternTable(data)
{
	(void)time;

	vrMode = vdp.getVRMode();
	setSizeMask(time);

	// Whole VRAM is cachable.
	// Because this window has no observer, any EmuTime can be passed.
	// TODO: Move this to cache registration.
	bitmapCacheWindow.setMask(0x1FFFF, -1 << 17, EmuTime::zero);
}

VDPVRAM::~VDPVRAM()
{
}

void VDPVRAM::clear()
{
	// Initialise VRAM data array.
	// TODO: Fill with checkerboard pattern NMS8250 has.
	memset(&data[0], 0, data.getSize());
	if (data.getSize() != actualSize) {
		assert(data.getSize() > actualSize);
		// Read from unconnected VRAM returns random data.
		// TODO reading same location multiple times does not always
		// give the same value.
		memset(&data[actualSize], 0xFF, data.getSize() - actualSize);
	}
}

void VDPVRAM::updateDisplayMode(DisplayMode mode, EmuTime::param time)
{
	renderer->updateDisplayMode(mode, time);
	cmdEngine->updateDisplayMode(mode, time);
	spriteChecker->updateDisplayMode(mode, time);
}

void VDPVRAM::updateDisplayEnabled(bool enabled, EmuTime::param time)
{
	assert(vdp.isInsideFrame(time));
	renderer->updateDisplayEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateDisplayEnabled(enabled, time);
}

void VDPVRAM::updateSpritesEnabled(bool enabled, EmuTime::param time)
{
	assert(vdp.isInsideFrame(time));
	renderer->updateSpritesEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateSpritesEnabled(enabled, time);
}

void VDPVRAM::setSizeMask(EmuTime::param time)
{
	unsigned vrMask = vrMode ? 0xffffffff // VR=1, no extra masking
	                         : 0x8000;    // VR=0, mask at 32kB
	sizeMask = std::min(vrMask, Math::powerOfTwo(actualSize)) - 1;

	cmdReadWindow.setSizeMask(sizeMask, time);
	cmdWriteWindow.setSizeMask(sizeMask, time);
	nameTable.setSizeMask(sizeMask, time);
	colorTable.setSizeMask(sizeMask, time);
	patternTable.setSizeMask(sizeMask, time);
	bitmapVisibleWindow.setSizeMask(sizeMask, time);
	bitmapCacheWindow.setSizeMask(sizeMask, time);
	spriteAttribTable.setSizeMask(sizeMask, time);
	spritePatternTable.setSizeMask(sizeMask, time);
}
static inline unsigned swapAddr(unsigned x)
{
	// translate VR0 address to corresponding VR1 address
	//  note: output bit 0 is always 1
	//        input bit 6 is taken twice
	//        only 15 bits of the input are used
	return 1 | ((x & 0x007F) << 1) | ((x & 0x7FC0) << 2);
}
void VDPVRAM::updateVRMode(bool newVRmode, EmuTime::param time)
{
	if (vrMode == newVRmode) {
		// The swapping below may only happen when the mode is
		// actually changed. So this test is not only an optimization.
		return;
	}
	vrMode = newVRmode;
	setSizeMask(time);

	if (vrMode) {
		// switch from VR=0 to VR=1
		for (int i = 0x7FFF; i >=0; --i) {
			std::swap(data[i], data[swapAddr(i)]);
		}
	} else {
		// switch from VR=1 to VR=0
		for (int i = 0; i < 0x8000; ++i) {
			std::swap(data[i], data[swapAddr(i)]);
		}
	}
}

void VDPVRAM::setRenderer(Renderer* renderer, EmuTime::param time)
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
	ar.serialize("baseMask",  origBaseMask);
	ar.serialize("indexMask", indexMask);
	if (ar.isLoader()) {
		effectiveBaseMask = origBaseMask & sizeMask;
		combiMask = ~effectiveBaseMask | indexMask;
		// TODO ?  observer->updateWindow(isEnabled(), time);
	}
}

template<typename Archive>
void VDPVRAM::serialize(Archive& ar, unsigned /*version*/)
{
	if (ar.isLoader()) {
		vrMode = vdp.getVRMode();
		setSizeMask(static_cast<MSXDevice&>(vdp).getCurrentTime());
	}

	ar.serialize_blob("data", &data[0], actualSize);
	ar.serialize("cmdReadWindow",       cmdReadWindow);
	ar.serialize("cmdWriteWindow",      cmdWriteWindow);
	ar.serialize("nameTable",           nameTable);
	// TODO: Find a way of changing the line below to "colorTable",
	// without breaking backwards compatibility
	ar.serialize("colourTable",         colorTable);
	ar.serialize("patternTable",        patternTable);
	ar.serialize("bitmapVisibleWindow", bitmapVisibleWindow);
	ar.serialize("bitmapCacheWindow",   bitmapCacheWindow);
	ar.serialize("spriteAttribTable",   spriteAttribTable);
	ar.serialize("spritePatternTable",  spritePatternTable);
}
INSTANTIATE_SERIALIZE_METHODS(VDPVRAM);

} // namespace openmsx
