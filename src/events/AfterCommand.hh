#ifndef AFTERCOMMAND_HH
#define AFTERCOMMAND_HH

#include "Command.hh"
#include "EventListener.hh"
#include "Event.hh"
#include <memory>
#include <vector>

namespace openmsx {

class Reactor;
class EventDistributor;
class CommandController;
class AfterCmd;

class AfterCommand final : public Command, private EventListener
{
public:
	using EventPtr = std::shared_ptr<const Event>;

	AfterCommand(Reactor& reactor,
	             EventDistributor& eventDistributor,
	             CommandController& commandController);
	~AfterCommand();

	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

private:
	template<typename PRED> void executeMatches(PRED pred);
	template<EventType T> void executeEvents();
	template<EventType T> void afterEvent(
	                   array_ref<TclObject> tokens, TclObject& result);
	void afterInputEvent(const EventPtr& event,
	                   array_ref<TclObject> tokens, TclObject& result);
	void afterTclTime (int ms,
	                   array_ref<TclObject> tokens, TclObject& result);
	void afterTime    (array_ref<TclObject> tokens, TclObject& result);
	void afterRealTime(array_ref<TclObject> tokens, TclObject& result);
	void afterIdle    (array_ref<TclObject> tokens, TclObject& result);
	void afterInfo    (array_ref<TclObject> tokens, TclObject& result);
	void afterCancel  (array_ref<TclObject> tokens, TclObject& result);

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	using AfterCmds = std::vector<std::unique_ptr<AfterCmd>>;
	AfterCmds afterCmds;
	Reactor& reactor;
	EventDistributor& eventDistributor;

	friend class AfterCmd;
	friend class AfterTimedCmd;
	friend class AfterRealTimeCmd;
};

} // namespace openmsx

#endif
