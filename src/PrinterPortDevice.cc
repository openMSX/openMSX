// $Id$

#include "PrinterPortDevice.hh"


const std::string &PrinterPortDevice::getClass()
{
	return className;
}

const std::string PrinterPortDevice::className("Printer Port");
