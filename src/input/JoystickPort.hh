// $Id$

#ifndef __JOYSTICKPORT_HH__
#define __JOYSTICKPORT_HH__

#include "openmsx.hh"
#include "Connector.hh"


namespace openmsx {

class JoystickPort : public Connector
{
	public:
		JoystickPort(const string &name, const EmuTime &time);
		virtual ~JoystickPort();

		virtual const string &getName() const;
		virtual const string &getClass() const;
		virtual void plug(Pluggable *device, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

		byte read(const EmuTime &time);
		void write(byte value, const EmuTime &time);

	private:
		class DummyJoystick* dummy;
		string name;

		byte lastValue;
};

} // namespace openmsx

#endif // __JOYSTICKPORT_HH__
