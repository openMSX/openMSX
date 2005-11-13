// $Id$

#ifndef PLUGGINGCONTROLLER_HH
#define PLUGGINGCONTROLLER_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class Connector;
class Pluggable;
class CliComm;
class Scheduler;
class CommandController;

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

	// Commands
	class PlugCmd : public SimpleCommand {
	public:
		PlugCmd(CommandController& commandController,
		        PluggingController& pluggingController);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& pluggingController;
	} plugCmd;

	class UnplugCmd : public SimpleCommand {
	public:
		UnplugCmd(CommandController& commandController,
		          PluggingController& pluggingController);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& pluggingController;
	} unplugCmd;

	class PluggableInfo : public InfoTopic {
	public:
		PluggableInfo(CommandController& commandController,
		              PluggingController& pluggingController);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& pluggingController;
	} pluggableInfo;

	class ConnectorInfo : public InfoTopic {
	public:
		ConnectorInfo(CommandController& commandController,
		              PluggingController& pluggingController);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& pluggingController;
	} connectorInfo;

	class ConnectionClassInfo : public InfoTopic {
	public:
		ConnectionClassInfo(CommandController& commandController,
		                    PluggingController& pluggingController);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		PluggingController& pluggingController;
	} connectionClassInfo;

	CliComm& cliComm;
	Scheduler& scheduler;
};

} // namespace openmsx

#endif
