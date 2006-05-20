// $Id$

#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "Renderer.hh"

namespace openmsx {

// class VRAMWindow:

static unsigned roundUpPow2(unsigned a)
{
	unsigned result = 1;
	while (result < a) result <<= 1;
	return result;
}

VRAMWindow::VRAMWindow(Ram& vram)
	: data(&vram[0])
	, sizeMask(roundUpPow2(vram.getSize()) - 1)
{
	observer = NULL;
	baseAddr  = -1; // disable window
	baseMask = 0;
	indexMask = 0; // these 3 don't matter but it makes valgrind happy
	combiMask = 0;
}


// class VDPVRAM:

static unsigned bufferSize(unsigned size)
{
	// for 16kb vram still allocate a 32kb buffer
	//  (mirroring happens at 32kb, upper 16kb contains random data)
	return (size != 0x4000) ? size : 0x8000;
}

VDPVRAM::VDPVRAM(VDP& vdp_, unsigned size, const EmuTime& time)
	: vdp(vdp_)
	, data(vdp.getMotherBoard(), "VRAM", "Video RAM.", bufferSize(size))
	, clock(time)
	, sizeMask(roundUpPow2(data.getSize()) - 1)
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

} // namespace openmsx
