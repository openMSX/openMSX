// $Id$

#include "VDPVRAM.hh"
#include "SpriteChecker.hh"

// class VDPVRAM:

VDPVRAM::VDPVRAM(int size) {
	this->size = size;

	// TODO: If this stays, initialise from constructor param.
	currentTime = 0ull;

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
	bitmapWindow.setData(data);
	spriteAttribTable.setData(data);
	spritePatternTable.setData(data);

}

VDPVRAM::~VDPVRAM() {
	delete[] data;
}

void VDPVRAM::updateDisplayMode(int mode, const EmuTime &time) {
	renderer->updateDisplayMode(mode, time);
	cmdEngine->updateDisplayMode(mode, time);
	spriteChecker->updateDisplayMode(mode, time);
}

void VDPVRAM::updateDisplayEnabled(bool enabled, const EmuTime &time) {
	renderer->updateDisplayEnabled(enabled, time);
	cmdEngine->sync(time);
	spriteChecker->updateDisplayEnabled(enabled, time);
}

void VDPVRAM::updateSpritesEnabled(bool enabled, const EmuTime &time) {
	cmdEngine->sync(time);
	spriteChecker->updateSpritesEnabled(enabled, time);
}

// class Window:

VDPVRAM::Window::Window() {
	observer = NULL;
	combiMask = 0;	// doesn't matter but makes valgrind happy
	baseAddr = -1;
}

