// $Id$

#incude "Debugger.hh"

Debugger * Debugger::instance()
{
	static Debugger * oneInstance;
	return oneInstance;
}
