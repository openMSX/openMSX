// $Id$

#include "PrinterPortDevice.hh"


const std::string &PrinterPortDevice::getClass()
{
	static const std::string className("Printer Port");
	return className;
}
