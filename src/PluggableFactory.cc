// $Id$

#include "config.h"

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
#ifdef	HAVE_SYS_SOCKET_H
	controller->registerPluggable(new JoyNet());
#endif
	controller->registerPluggable(new KeyJoystick());
	Joystick::registerAll(controller);
}

} // namespace openmsx
