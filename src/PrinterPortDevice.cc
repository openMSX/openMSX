// $Id$

#include "PrinterPortDevice.hh"


namespace openmsx {

const string &PrinterPortDevice::getClass() const
{
	static const string className("Printer Port");
	return className;
}

} // namespace openmsx
