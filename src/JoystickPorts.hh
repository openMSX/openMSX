
#ifndef __JOYSTICKPORTS_HH__
#define __JOYSTICKPORTS_HH__

#include "openmsx.hh"
#include "JoystickDevice.hh"

class JoystickPorts
{
	public:
		~JoystickPorts();
		static JoystickPorts *instance();

		byte read();
		void write(byte value);
		void plug(int port, JoystickDevice *device);
		void unplug(int port);

	private:
		JoystickPorts();
		static JoystickPorts *oneInstance;

		int selectedPort;
		JoystickDevice* ports[2];
};
#endif
