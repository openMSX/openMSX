// $Id$

#ifndef __DEBUGINTERFACE_HH__
#define __DEBUGINTERFACE_HH__

#include "openmsx.hh"
#include <map>
#include <string>
#include <cassert>

class DebugInterface
{
	public:
		DebugInterface();
		virtual ~DebugInterface();
		virtual std::string getRegisterName (word regNr){assert (false);};
		virtual word getRegisterNumber (std::string regName){assert (false);};
		virtual dword getDataSize ()=0;
		virtual byte readDebugData (dword address)=0;
		virtual std::string getDeviceName ()=0;
		virtual bool getRegisters (std::map < std::string, word> & regMap)=0; //obsolete
		void registerDevice ();
};

#endif
