// $Id$

/** V9990 VRAM
  *
  * Video RAM for the V9990
  */

#include <cassert>
#include "Scheduler.hh"
#include "Debugger.hh"

#include "V9990VRAM.hh"

namespace openmsx {
static const unsigned VRAM_SIZE = 512 * 1024; // 512kB
static const char*    DEBUG_ID  = "V9990 VRAM";
	
// -------------------------------------------------------------------------
// Constructor & Destructor
// -------------------------------------------------------------------------

V9990VRAM::V9990VRAM(V9990 *v9990, const EmuTime& time)
	{
		this->v9990 = v9990;
		vram = new byte[VRAM_SIZE];
		memset(vram, 0, VRAM_SIZE);
		
		Debugger::instance().registerDebuggable(DEBUG_ID, *this);
	}

V9990VRAM::~V9990VRAM()
{
	Debugger::instance().unregisterDebuggable(DEBUG_ID, *this);
	delete[] vram;
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
	return vram[address];
}

void V9990VRAM::write(unsigned address, byte value)
{
	vram[address] = value;
}

// -------------------------------------------------------------------------
// V9990VRAM
// -------------------------------------------------------------------------

byte V9990VRAM::readVRAM(unsigned address, EmuTime& time)
{
	return vram[address];
}

void V9990VRAM::writeVRAM(unsigned address, byte value, EmuTime& time)
{
	vram[address] = value;
}
	
} // namespace openmsx
