// $Id$

#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "CommandController.hh"
#include "EmuTime.hh"

#include <fstream>
using std::ofstream;


namespace openmsx {

// class VRAMWindow:

VRAMWindow::VRAMWindow() {
	observer = NULL;
	baseAddr = -1;
	combiMask = 0;	// doesn't matter but makes valgrind happy
	baseMask = 0;	// doesn't matter but makes valgrind happy
}


// class VDPVRAM:

VDPVRAM::VDPVRAM(VDP *vdp, int size)
	: dumpVRAMCmd(this)
{
	this->vdp = vdp;
	this->size = size;

	// TODO: If this stays, initialise from constructor param.
	currentTime(0ull);

	// Initialise VRAM data array.
	data = new byte[size];
	// TODO: Fill with checkerboard pattern NMS8250 has.
	memset(data, 0, size);

	// Initialise access windows.
	// TODO: Use vram.read(window, index) instead of vram.window.read(index)?
	//       The former doesn't need setData.
	cmdReadWindow.setData(data);
	cmdWriteWindow.setData(data);
	nameTable.setData(data);
	colourTable.setData(data);
	patternTable.setData(data);
	bitmapVisibleWindow.setData(data);
	bitmapCacheWindow.setData(data);
	spriteAttribTable.setData(data);
	spritePatternTable.setData(data);

	// Whole VRAM is cachable.
	// Because this window has no observer, any EmuTime can be passed.
	// TODO: Move this to cache registration.
	bitmapCacheWindow.setMask(0x1FFFF, -1 << 17, EmuTime::zero);

	CommandController::instance()->registerCommand(&dumpVRAMCmd, "vramdump");
}

VDPVRAM::~VDPVRAM()
{
	CommandController::instance()->unregisterCommand(&dumpVRAMCmd, "vramdump");

	delete[] data;
}

void VDPVRAM::updateDisplayMode(DisplayMode mode, const EmuTime &time) {
	renderer->updateDisplayMode(mode, time);
	cmdEngine->updateDisplayMode(mode, time);
	spriteChecker->updateDisplayMode(mode, time);
}

void VDPVRAM::updateDisplayEnabled(bool enabled, const EmuTime &time) {
	assert(vdp->isInsideFrame(time));
	renderer->updateDisplayEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateDisplayEnabled(enabled, time);
}

void VDPVRAM::updateSpritesEnabled(bool enabled, const EmuTime &time) {
	assert(vdp->isInsideFrame(time));
	renderer->updateSpritesEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateSpritesEnabled(enabled, time);
}

void VDPVRAM::setRenderer(Renderer *renderer, const EmuTime &time) {
	this->renderer = renderer;

	bitmapVisibleWindow.resetObserver();
	// Set up bitmapVisibleWindow to full VRAM.
	// TODO: Have VDP/Renderer set the actual range.
	bitmapVisibleWindow.setMask(0x1FFFF, -1 << 17, time);
	// TODO: If it is a good idea to send an initial sync,
	//       then call setObserver before setMask.
	bitmapVisibleWindow.setObserver(renderer);
}


// class DumpVRAMCmd

VDPVRAM::DumpVRAMCmd::DumpVRAMCmd(VDPVRAM *vram_)
	: vram(vram_)
{
}

string VDPVRAM::DumpVRAMCmd::execute(const vector<string> &tokens)
{
	ofstream outfile("vramdump", ofstream::binary);
	outfile.write((char*)vram->data, vram->size);
	return "";
}

string VDPVRAM::DumpVRAMCmd::help(const vector<string> &tokens) const
{
	return "Dump vram content to file \"vramdump\"\n";
}

} // namespace openmsx

