// $Id$

#include "Y8950KeyboardDevice.hh"


const string &Y8950KeyboardDevice::getClass() const
{
	static const string className("Y8950 Keyboard Port");
	return className;
}
