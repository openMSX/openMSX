// $Id$

#ifndef __DEBUGINTERFACE_HH__
#define __DEBUGINTERFACE_HH__

#include "openmsx.hh"
#include <map>
#include <string>

class DebugInterface
{
	public:
		DebugInterface();
		virtual ~DebugInterface();
		virtual bool getRegisters (std::map < std::string, word> & regMap)=0;
	
};

#endif
