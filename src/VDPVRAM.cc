// $Id$

#include "VDPVRAM.hh"

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

VDPVRAM::Window::Window() {
	disable();
}

void VDPVRAM::Window::setData(byte *data) {
	this->data = data;
}

