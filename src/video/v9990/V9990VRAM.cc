// $Id$

/** V9990 VRAM
  *
  * Video RAM for the V9990
  */

#include "Debugger.hh"
#include "V9990VRAM.hh"

using std::string;

namespace openmsx {

static const unsigned VRAM_SIZE = 512 * 1024; // 512kB
static const char*    DEBUG_ID  = "V9990 VRAM";
	
// -------------------------------------------------------------------------
// Constructor & Destructor
// -------------------------------------------------------------------------

V9990VRAM::V9990VRAM(V9990 *vdp_, const EmuTime& time)
	: vdp(vdp_)
{
	data = new byte[VRAM_SIZE];
	memset(data, 0, VRAM_SIZE);
	
	Debugger::instance().registerDebuggable(DEBUG_ID, *this);
}

V9990VRAM::~V9990VRAM()
{
	Debugger::instance().unregisterDebuggable(DEBUG_ID, *this);
	delete[] data;
}

// -------------------------------------------------------------------------
// V9990VRAM
// -------------------------------------------------------------------------

static unsigned mapAddress(unsigned address, V9990DisplayMode mode) {
	address &= 0x7FFFF;
	switch(mode) {
		case P1:
			break;
		case P2:
			if(address < 0x7BE00) {
				address = ((address >>16) & 0x1) |
				          ((address << 1) & 0x7FFFF);
			} else if(address < 0x7C000) {
				address &= 0x3FFFF;
			} /* else { address = address; } */
			break;
		default /* Bx */:
			address = ((address >> 18) & 0x1) |
			          ((address <<  1) & 0x7FFFE);
	}
	return address;
}

/*inline*/ byte V9990VRAM::readVRAM(unsigned address)
{
	return data[mapAddress(address, vdp->getDisplayMode())];
}

/*inline*/ void V9990VRAM::writeVRAM(unsigned address, byte value)
{
	data[mapAddress(address, vdp->getDisplayMode())] = value;
}

// -------------------------------------------------------------------------
// Debuggable
// -------------------------------------------------------------------------
	
unsigned int V9990VRAM::getSize() const
{
	return VRAM_SIZE;
}

const string& V9990VRAM::getDescription() const
{
	static const string desc = "V9990 Video RAM";
	return desc;
}

byte V9990VRAM::read(unsigned address)
{
	return data[mapAddress(address, vdp->getDisplayMode())];
}

void V9990VRAM::write(unsigned address, byte value)
{
	data[mapAddress(address, vdp->getDisplayMode())] = value;
}

} // namespace openmsx
