// $Id$

#include "PrinterPortDevice.hh"

using std::string;

namespace openmsx {

const string &PrinterPortDevice::getClass() const
{
	static const string className("Printer Port");
	return className;
}

} // namespace openmsx
