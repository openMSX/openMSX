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
	for (int i = 0; i < NUM_WINDOWS; i++) {
		windows[i].setData(data);
	}
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

