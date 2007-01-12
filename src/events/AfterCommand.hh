// $Id$

#ifndef AFTERCOMMAND_HH
#define AFTERCOMMAND_HH

#include "Command.hh"
#include "EventListener.hh"
#include "Event.hh"
#include <map>

namespace openmsx {

class Reactor;
class EventDistributor;
class CommandController;
class AfterCmd;

class AfterCommand : public SimpleCommand, private EventListener
{
public:
	AfterCommand(Reactor& reactor,
	             EventDistributor& eventDistributor,
	             CommandController& commandController);
	virtual ~AfterCommand();

	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	std::string afterTime(const std::vector<std::string>& tokens);
	std::string afterIdle(const std::vector<std::string>& tokens);
	std::string afterInfo(const std::vector<std::string>& tokens);
	std::string afterCancel(const std::vector<std::string>& tokens);
	template<EventType T> std::string afterEvent(
		const std::vector<std::string>& tokens);
	template<EventType T> void executeEvents();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	typedef std::map<std::string, AfterCmd*> AfterCmdMap;
	AfterCmdMap afterCmds;

	Reactor& reactor;
	EventDistributor& eventDistributor;

	friend class AfterCmd;
};

} // namespace openmsx

#endif
