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
		virtual std::string getRegisterName (word regNr) {
			assert(false);
			return "";
		}
		virtual word getRegisterNumber (std::string regName){
			assert(false);
			return 0;
		}
		virtual dword getDataSize ()=0;
		virtual byte readDebugData (dword address)=0;
		virtual std::string getDeviceName ()=0;
		void registerDevice ();
};

#endif
