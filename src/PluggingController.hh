// $Id$

#ifndef PLUGGINGCONTROLLER_HH
#define PLUGGINGCONTROLLER_HH

#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Connector;
class Pluggable;
class CliComm;
class Scheduler;
class CommandController;
class PlugCmd;
class UnplugCmd;
class PluggableInfo;
class ConnectorInfo;
class ConnectionClassInfo;

/**
 * Central administration of Connectors and Pluggables.
 */
class PluggingController
{
public:
	explicit PluggingController(MSXMotherBoard& motherBoard);
	~PluggingController();

	/** Connectors must be (un)registered
	  */
	void registerConnector(Connector& connector);
	void unregisterConnector(Connector& connector);

	/**
	 * Add a Pluggable to the registry.
	 * PluggingController has ownership of all registered Pluggables.
	 */
	void registerPluggable(Pluggable* pluggable);

	/**
	 * Removes a Pluggable from the registry.
	 * If you attempt to unregister a Pluggable that is not in the registry,
	 * nothing happens.
	 */
	void unregisterPluggable(Pluggable* pluggable);

private:
	Connector* getConnector(const std::string& name);
	Pluggable* getPluggable(const std::string& name);

	typedef std::vector<Connector*> Connectors;
	Connectors connectors;
	typedef std::vector<Pluggable*> Pluggables;
	Pluggables pluggables;

	friend class PlugCmd;
	friend class UnplugCmd;
	friend class PluggableInfo;
	friend class ConnectorInfo;
	friend class ConnectionClassInfo;
	const std::auto_ptr<PlugCmd> plugCmd;
	const std::auto_ptr<UnplugCmd> unplugCmd;
	const std::auto_ptr<PluggableInfo> pluggableInfo;
	const std::auto_ptr<ConnectorInfo> connectorInfo;
	const std::auto_ptr<ConnectionClassInfo> connectionClassInfo;

	CliComm& cliComm;
	Scheduler& scheduler;
};

} // namespace openmsx

#endif
