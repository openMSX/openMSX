namespace openmsx {

// $Id$

class Debugger
{
	public:
		Debugger * instance ();
		void registerDevice (std::string, DebugInterface * interface);
		void unregisterDevice (std::string);
		DebugInterface * getInterfaceByName (std::string);

	private:
		Debugger();
		~Debugger();
}

} // namespace openmsx
