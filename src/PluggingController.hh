// $Id$

#ifndef __PLUGGING_CONTROLLER__
#define __PLUGGING_CONTROLLER__

#include <vector>
#include "ConsoleSource/ConsoleCommand.hh"

//forward declarations
class Connector;
class Pluggable;


class PluggingController {
public:
	/**
	 * Destructor
	 */
	~PluggingController();

	/**
	 * This class is a singleton
	 */
	static PluggingController* instance();
	
	/**
	 * Connectors can be (un)registered
	 */
	void registerConnector(Connector *connector);
        void unregisterConnector(Connector *connector);
	
	/**
	 * Pluggables can be (un)registered
	 */
	void registerPluggable(Pluggable *pluggable);
	void unregisterPluggable(Pluggable *pluggable);

private:
	PluggingController();

	static PluggingController *oneInstance;
	vector<Connector*> connectors;
	vector<Pluggable*> pluggables;

	// ConsoleCommands
	class PlugCmd : public ConsoleCommand {
	public:
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens);
	};
	friend class PlugCmd;
	PlugCmd plugCmd;
	class UnplugCmd : public ConsoleCommand {
	public:
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens);
	};
	friend class UnplugCmd;
	UnplugCmd unplugCmd;
};

#endif //__PLUGGING_CONTROLLER__
