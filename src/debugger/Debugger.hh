// $Id$

#include <string>

using std::string;

namespace openmsx {

class DebugInterface;

class Debugger
{
	public:
		Debugger& instance();
		void registerDevice  (const string& name, DebugInterface& interface);
		void unregisterDevice(const string& name, DebugInterface& interface);
		DebugInterface* getInterfaceByName(const string& name);

	private:
		Debugger();
		~Debugger();
};

} // namespace openmsx

