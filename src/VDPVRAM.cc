// $Id$

#include "VDPVRAM.hh"

VDPVRAM::VDPVRAM(int size) {
	this->size = size;

	// Initialise VRAM data array.
	data = new byte[size];
	// TODO: Fill with checkerboard pattern NMS8250 has.
	memset(data, 0, size);

	// Initialise access windows.
	for (int i = 0; i < NUM_READ_WINDOWS; i++) {
		readWindows[i].setData(data);
	}
	writeWindow.setData(data);
}

VDPVRAM::~VDPVRAM() {
	delete[] data;
}

VDPVRAM::Window::Window() {
	this->enabled = false;
}

void VDPVRAM::Window::setData(byte *data) {
	this->data = data;
}

