// $Id $

#include "V9990VRAMObserver.hh"

namespace openmsx {

/** Default Observer observes all of VRAM
  */
V9990VRAMObserver::V9990VRAMObserver()
	: startAddress(0)
	, size(VRAM_SIZE)
{
}

/** Observer with specified window
  */
V9990VRAMObserver::V9990VRAMObserver(
	unsigned startAddress_, unsigned size_)
	: startAddress(startAddress_)
	, size(size_)
{
}

} // namespace
