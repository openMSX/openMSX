// $Id$
 
#include "MSXIODevice.hh"

byte MSXIODevice:: readIO(byte port, const EmuTime &time)
{
	return 255;
}
void MSXIODevice::writeIO(byte port, byte value, const EmuTime &time)
{
	// do nothing
}
