// $Id$

#incude "Debugger.hh"

namespace openmsx {

Debugger * Debugger::instance()
{
	static Debugger * oneInstance;
	return oneInstance;
}

} // namespace openmsx
