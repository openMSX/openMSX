// $Id$

#ifndef __DEBUGINTERFACE_HH__
#define __DEBUGINTERFACE_HH__

#include "openmsx.hh"
#include <string>
#include <cassert>

using std::string;

namespace openmsx {

class DebugInterface
{
	public:
		DebugInterface();
		virtual ~DebugInterface();
		virtual string getRegisterName(word regNr) {
			assert(false);
			return "";
		}
		virtual word getRegisterNumber(string regName) {
			assert(false);
			return 0;
		}
		virtual dword getDataSize() = 0;
		virtual byte readDebugData(dword address) = 0;
		virtual string getDeviceName() = 0;
		void registerDevice();
};

} // namespace openmsx

#endif
