// $Id$

/** V9990 VRAM
  *
  * Video RAM for the V9990
  */

#include "V9990.hh"
#include "V9990VRAM.hh"
#include "V9990VRAMObserver.hh"
#include <algorithm>

namespace openmsx {

V9990VRAM::V9990VRAM(V9990& vdp_, const EmuTime& /*time*/)
	: vdp(vdp_)
	, data(vdp.getMotherBoard(), "V9990 VRAM", "V9990 Video RAM", VRAM_SIZE)
{
	memset(&data[0], 0, data.getSize());
}

static unsigned interleave(unsigned address)
{
	return ((address & 1) << 18) | ((address & 0x7FFFE) >> 1);
}

static unsigned mapAddress(unsigned address, V9990DisplayMode mode)
{
	address &= 0x7FFFF;
	switch (mode) {
		case P1:
			break;
		case P2:
			if (address < 0x7BE00) {
				address = ((address >>16) & 0x1) |
				          ((address << 1) & 0x7FFFF);
			} else if (address < 0x7C000) {
				address &= 0x3FFFF;
			} /* else { address = address; } */
			break;
		default /* Bx */:
			address = interleave(address);
	}
	return address;
}

byte V9990VRAM::readVRAM(unsigned address)
{
	return data[mapAddress(address, vdp.getDisplayMode())];
}

void V9990VRAM::writeVRAM(unsigned address, byte value)
{
	data[mapAddress(address, vdp.getDisplayMode())] = value;
	notifyObservers(address);
}

byte V9990VRAM::readVRAMInterleave(unsigned address)
{
	return data[interleave(address)];
}

void V9990VRAM::writeVRAMInterleave(unsigned address, byte value)
{
	data[interleave(address)] = value;
}

void V9990VRAM::addObserver(V9990VRAMObserver& observer)
{
	assert(std::find(observers.begin(), observers.end(), &observer)
	       == observers.end());
	observers.push_back(&observer);
}

void V9990VRAM::removeObserver(V9990VRAMObserver& observer)
{
	assert(std::find(observers.begin(), observers.end(), &observer)
	       != observers.end());
	observers.erase(std::find(observers.begin(), observers.end(), &observer));
}

void V9990VRAM::notifyObservers(unsigned address)
{
	for (V9990VRAMObservers::iterator it = observers.begin();
	     it != observers.end(); ++it) {
		if ((*it)->isInWindow(address)) {
			(*it)->updateVRAM(address);
		}
	}
}

} // namespace openmsx
