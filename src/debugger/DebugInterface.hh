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
		virtual ~DebugInterface();

		virtual dword getDataSize() const = 0;
		virtual const string getRegisterName(dword regNr) const;
		virtual dword getRegisterNumber(const string& regName) const;
		virtual byte readDebugData(dword address) const = 0;
		virtual const string& getDeviceName() const = 0;

		void registerDevice();

	protected:
		DebugInterface();
};

} // namespace openmsx

#endif
