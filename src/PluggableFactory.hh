// $Id$

#ifndef __PLUGGABLEFACTORY_HH__
#define __PLUGGABLEFACTORY_HH__

namespace openmsx {

class PluggingController;

class PluggableFactory
{
public:
	static void createAll(PluggingController *controller);
};

} // namespace openmsx

#endif // __PLUGGABLEFACTORY_HH__
