// $Id$

#include <string>

using std::string;

namespace openmsx {

class Debugger
{
	public:
		Debugger* instance();
		void registerDevice(string, DebugInterface* interface);
		void unregisterDevice(string);
		DebugInterface* getInterfaceByName(string);

	private:
		Debugger();
		~Debugger();
};

} // namespace openmsx

