// $Id$

#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "Renderer.hh"

namespace openmsx {

// class VRAMWindow:

VRAMWindow::VRAMWindow()
{
	observer = NULL;
	baseAddr  = -1; // disable window
	baseMask = 0;
	indexMask = 0; // these 3 don't matter but it makes valgrind happy
	combiMask = 0;
}


// class VDPVRAM:

VDPVRAM::VDPVRAM(VDP& vdp_, unsigned size, const EmuTime& time)
	: vdp(vdp_)
	, data(vdp.getMotherBoard(), "VRAM", "Video RAM.", size)
	, clock(time)
{
	// Initialise VRAM data array.
	// TODO: Fill with checkerboard pattern NMS8250 has.
	memset(&data[0], 0, data.getSize());

	// Initialise access windows.
	// TODO: Use vram.read(window, index) instead of vram.window.read(index)?
	//       The former doesn't need setData.
	cmdReadWindow.setData(&data[0]);
	cmdWriteWindow.setData(&data[0]);
	nameTable.setData(&data[0]);
	colourTable.setData(&data[0]);
	patternTable.setData(&data[0]);
	bitmapVisibleWindow.setData(&data[0]);
	bitmapCacheWindow.setData(&data[0]);
	spriteAttribTable.setData(&data[0]);
	spritePatternTable.setData(&data[0]);

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
