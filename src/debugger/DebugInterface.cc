// $Id$

#include <sstream>
#include <cstdlib>
#include "DebugInterface.hh"

using std::ostringstream;

namespace openmsx {

DebugInterface::DebugInterface ()
{
	// Debugger::instance()->register
}

DebugInterface::~DebugInterface()
{
}

void DebugInterface::registerDevice()
{
	// Debugger::instance()->registerDevice(getName (),this);
}


const string DebugInterface::getRegisterName(dword regNr) const
{
	ostringstream out;
	out << hex << regNr;
	return "0x" + out.str();
}

dword DebugInterface::getRegisterNumber(const string& regName) const
{
	return strtoul(regName.c_str(), NULL, 0);
}

} // namespace openmsx
