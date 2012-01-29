// $Id$

#ifndef PLUGGINGCONTROLLER_HH
#define PLUGGINGCONTROLLER_HH

#include "noncopyable.hh"
#include <vector>
#include <string>
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
	  * NULL if there is none with this name.
	  */
	Connector* findConnector(const std::string& name) const;

	/** Add a Pluggable to the registry.
	 */
	void registerPluggable(std::auto_ptr<Pluggable> pluggable);

	/** Return the Pluggable with given name or
	  * NULL if there is none with this name.
	  */
	Pluggable* findPluggable(const std::string& name) const;

	/**
	 * Access to the MSX specific CliComm, so that Connectors can get it.
	 */
	CliComm& getCliComm();

private:
	Connector& getConnector(const std::string& name) const;
	Pluggable& getPluggable(const std::string& name) const;

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
};

} // namespace openmsx

#endif
