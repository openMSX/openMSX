// $Id$

#include <fstream>
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "CommandController.hh"


// class VDPVRAM:

VDPVRAM::VDPVRAM(VDP *vdp, int size)
#ifdef DEBUG
	: dumpVRAMCmd(this)
#endif
{
	this->vdp = vdp;
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

	#ifdef DEBUG
	CommandController::instance()->registerCommand(&dumpVRAMCmd, "vramdump");
	#endif
}

VDPVRAM::~VDPVRAM()
{
	#ifdef DEBUG
	CommandController::instance()->unregisterCommand(&dumpVRAMCmd, "vramdump");
	#endif
	
	delete[] data;
}

void VDPVRAM::updateDisplayMode(int mode, const EmuTime &time) {
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
	cmdEngine->sync(time);
	spriteChecker->updateSpritesEnabled(enabled, time);
}

// class Window:

VDPVRAM::Window::Window() {
	observer = NULL;
	combiMask = 0;	// doesn't matter but makes valgrind happy
	baseAddr = -1;
}



#ifdef DEBUG

// class DumpVRAMCmd

void VDPVRAM::DumpVRAMCmd::execute(const std::vector<std::string> &tokens,
                                   const EmuTime &time)
{
	std::ofstream outfile("vramdump", std::ofstream::binary);
	outfile.write((char*)vram->data, vram->size);
}

void VDPVRAM::DumpVRAMCmd::help(const std::vector<std::string> &tokens) const
{
	print("[DEBUG] Dump vram content to file \"vramdump\"");
}

#endif
