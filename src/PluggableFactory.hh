// $Id$

#ifndef PLUGGABLEFACTORY_HH
#define PLUGGABLEFACTORY_HH

namespace openmsx {

class PluggingController;

class PluggableFactory
{
public:
	static void createAll(PluggingController* controller);
};

} // namespace openmsx

#endif
