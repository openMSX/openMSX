// $Id$

#ifndef __PLUGGING_CONTROLLER__
#define __PLUGGING_CONTROLLER__

#include "Command.hh"
#include <vector>

using std::vector;

namespace openmsx {

class Connector;
class Pluggable;


/**
 * Central administration of Connectors and Pluggables.
 */
class PluggingController
{
public:
	static PluggingController *instance();

	/**
	 * Connectors can be (un)registered
	 * Note: it is not an error when you try to unregister a Connector
	 *       that was not registered before, in this case nothing happens
	 *
	 */
	void registerConnector(Connector *connector);
	void unregisterConnector(Connector *connector);

	/**
	 * Add a Pluggable to the registry.
	 * PluggingController has ownership of all registered Pluggables.
	 */
	void registerPluggable(Pluggable *pluggable);

	/**
	 * Removes a Pluggable from the registry.
	 * If you attempt to unregister a Pluggable that is not in the registry,
	 * nothing happens.
	 */
	void unregisterPluggable(Pluggable *pluggable);

private:
	PluggingController();
	~PluggingController();

	Connector *getConnector(const string &name);
	Pluggable *getPluggable(const string &name);

	vector<Connector *> connectors;
	vector<Pluggable *> pluggables;

	// Commands
	class PlugCmd : public Command {
	public:
		virtual void execute(const vector<string> &tokens);
		virtual void help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	} plugCmd;
	friend class PlugCmd;

	class UnplugCmd : public Command {
	public:
		virtual void execute(const vector<string> &tokens);
		virtual void help   (const vector<string> &tokens) const;
		virtual void tabCompletion(vector<string> &tokens) const;
	} unplugCmd;
	friend class UnplugCmd;
};

} // namespace openmsx

#endif //__PLUGGING_CONTROLLER__
