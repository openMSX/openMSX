// $Id$

#ifndef AFTERCOMMAND_HH
#define AFTERCOMMAND_HH

#include "Command.hh"
#include "EventListener.hh"
#include "Event.hh"
#include <vector>

namespace openmsx {

class Reactor;
class EventDistributor;
class CommandController;
class AfterCmd;
class Event;

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
	template<EventType T> void executeEvents();
	template<EventType T> std::string afterEvent(
		const std::vector<std::string>& tokens);
	std::string afterTime(const std::vector<std::string>& tokens);
	std::string afterRealTime(const std::vector<std::string>& tokens);
	std::string afterIdle(const std::vector<std::string>& tokens);
	std::string afterInfo(const std::vector<std::string>& tokens);
	std::string afterCancel(const std::vector<std::string>& tokens);
	void executeRealTime();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	typedef std::vector<shared_ptr<AfterCmd> > AfterCmds;
	AfterCmds afterCmds;
	Reactor& reactor;
	EventDistributor& eventDistributor;

	friend class AfterCmd;
};

} // namespace openmsx

#endif
