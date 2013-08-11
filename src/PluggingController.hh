#ifndef PLUGGINGCONTROLLER_HH
#define PLUGGINGCONTROLLER_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Connector;
class Pluggable;
class CliComm;
class PlugCmd;
class UnplugCmd;
class PluggableInfo;
class ConnectorInfo;
class ConnectionClassInfo;

/**
 * Central administration of Connectors and Pluggables.
 */
class PluggingController : private noncopyable
{
public:
	explicit PluggingController(MSXMotherBoard& motherBoard);
	~PluggingController();

	/** Connectors must be (un)registered
	  */
	void registerConnector(Connector& connector);
	void unregisterConnector(Connector& connector);

	/** Return the Connector with given name or
	  * nullptr if there is none with this name.
	  */
	Connector* findConnector(string_ref name) const;

	/** Add a Pluggable to the registry.
	 */
	void registerPluggable(std::unique_ptr<Pluggable> pluggable);

	/** Return the Pluggable with given name or
	  * nullptr if there is none with this name.
	  */
	Pluggable* findPluggable(string_ref name) const;

	/** Access to the MSX specific CliComm, so that Connectors can get it.
	 */
	CliComm& getCliComm();

	/** Convenience method: get current time.
	 */
	EmuTime::param getCurrentTime() const;

private:
	Connector& getConnector(string_ref name) const;
	Pluggable& getPluggable(string_ref name) const;

	std::vector<Connector*> connectors;
	std::vector<std::unique_ptr<Pluggable>> pluggables;

	friend class PlugCmd;
	friend class UnplugCmd;
	friend class PluggableInfo;
	friend class ConnectorInfo;
	friend class ConnectionClassInfo;
	const std::unique_ptr<PlugCmd> plugCmd;
	const std::unique_ptr<UnplugCmd> unplugCmd;
	const std::unique_ptr<PluggableInfo> pluggableInfo;
	const std::unique_ptr<ConnectorInfo> connectorInfo;
	const std::unique_ptr<ConnectionClassInfo> connectionClassInfo;

	MSXMotherBoard& motherBoard;
};

} // namespace openmsx

#endif
