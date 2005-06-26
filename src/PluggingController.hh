// $Id$

#ifndef PLUGGINGCONTROLLER_HH
#define PLUGGINGCONTROLLER_HH

#include <vector>
#include "Command.hh"
#include "InfoTopic.hh"

namespace openmsx {

class MSXMotherBoard;
class Connector;
class Pluggable;
class Scheduler;
class CommandController;
class InfoCommand;

/**
 * Central administration of Connectors and Pluggables.
 */
class PluggingController
{
public:
	PluggingController(MSXMotherBoard& motherBoard);
	~PluggingController();

	/**
	 * Connectors can be (un)registered
	 * Note: it is not an error when you try to unregister a Connector
	 *       that was not registered before, in this case nothing happens
	 *
	 */
	void registerConnector(Connector* connector);
	void unregisterConnector(Connector* connector);

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

	// Commands
	class PlugCmd : public SimpleCommand {
	public:
		PlugCmd(PluggingController& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& parent;
	} plugCmd;

	class UnplugCmd : public SimpleCommand {
	public:
		UnplugCmd(PluggingController& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& parent;
	} unplugCmd;

	class PluggableInfo : public InfoTopic {
	public:
		PluggableInfo(PluggingController& parent);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& parent;
	} pluggableInfo;

	class ConnectorInfo : public InfoTopic {
	public:
		ConnectorInfo(PluggingController& parent);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& parent;
	} connectorInfo;

	class ConnectionClassInfo : public InfoTopic {
	public:
		ConnectionClassInfo(PluggingController& parent);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& parent;
	} connectionClassInfo;

	Scheduler& scheduler;
	CommandController& commandController;
	InfoCommand& infoCommand;
};

} // namespace openmsx

#endif
