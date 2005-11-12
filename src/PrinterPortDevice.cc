// $Id$

#include "PrinterPortDevice.hh"

namespace openmsx {

const std::string& PrinterPortDevice::getClass() const
{
	static const std::string className("Printer Port");
	return className;
}

} // namespace openmsx
