// $Id$

#ifndef __JOYSTICKPORTS_HH__
#define __JOYSTICKPORTS_HH__

#include "openmsx.hh"
#include "ConsoleSource/ConsoleCommand.hh"

// forward declaration
class JoystickDevice;
class Mouse;
class Joystick;


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

		// ConsoleCommands
		class JoyPortCmd : public ConsoleCommand {
		public:
			JoyPortCmd();
			virtual ~JoyPortCmd();
			virtual void execute(char *commandLine);
			virtual void help(char *commandLine);
			
			static const int NUM_JOYSTICKS = 9;
			Mouse *mouse;
			Joystick *joystick[NUM_JOYSTICKS];
		};
		JoyPortCmd joyPortCmd;
};
#endif
