// $Id$

#include "PrinterPortDevice.hh"


const string &PrinterPortDevice::getClass() const
{
	static const string className("Printer Port");
	return className;
}
