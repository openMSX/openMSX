// $Id$

#include "PluggableFactory.hh"
#include "PluggingController.hh"

#include "Joystick.hh"
#include "Mouse.hh"
#include "KeyJoystick.hh"
#include "JoyNet.hh"


namespace openmsx {

void PluggableFactory::createAll(PluggingController *controller)
{
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hotplugged (USB)

	controller->registerPluggable(new Mouse());
#ifndef	NO_SOCKET
	controller->registerPluggable(new JoyNet());
#endif
	controller->registerPluggable(new KeyJoystick());
	try {
		for (int i = 0; ; i++) {
			controller->registerPluggable(new Joystick(i));
		}
	} catch (JoystickException &e) {
		// No more joysticks.
		// TODO: Don't use exceptions for this.
		//       Instead, make a single method to register all.
	}
}

} // namespace openmsx
