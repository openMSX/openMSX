#ifndef PLUGGABLEFACTORY_HH
#define PLUGGABLEFACTORY_HH

namespace openmsx {

class PluggingController;
class MSXMotherBoard;

class PluggableFactory
{
public:
	static void createAll(PluggingController& controller,
	                      MSXMotherBoard& motherBoard);
};

} // namespace openmsx

#endif
