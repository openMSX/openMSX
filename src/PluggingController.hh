// $Id$

#ifndef __PLUGGING_CONTROLLER__
#define __PLUGGING_CONTROLLER__

#include "Command.hh"

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
	 * Note: it is not an error when you try to unregister a Connector
	 *       that was not registered before, in this case nothing happens
	 *       
	 */
	void registerConnector(Connector *connector);
        void unregisterConnector(Connector *connector);
	
	/**
	 * Pluggables can be (un)registered
	 * Note: it is not an error when you try to unregister a Pluggable
	 *       that was not registered before, in this case nothing happens
	 */
	void registerPluggable(Pluggable *pluggable);
	void unregisterPluggable(Pluggable *pluggable);

private:
	PluggingController();

	std::vector<Connector*> connectors;
	std::vector<Pluggable*> pluggables;

	// Commands
	class PlugCmd : public Command {
	public:
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help   (const std::vector<std::string> &tokens) const;
		virtual void tabCompletion(std::vector<std::string> &tokens) const;
	};
	friend class PlugCmd;
	PlugCmd plugCmd;
	class UnplugCmd : public Command {
	public:
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help   (const std::vector<std::string> &tokens) const;
		virtual void tabCompletion(std::vector<std::string> &tokens) const;
	};
	friend class UnplugCmd;
	UnplugCmd unplugCmd;
};

#endif //__PLUGGING_CONTROLLER__
