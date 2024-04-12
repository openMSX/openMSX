#ifndef AFTERCOMMAND_HH
#define AFTERCOMMAND_HH

#include "Command.hh"
#include "EventListener.hh"
#include "Event.hh"

#include <concepts>
#include <vector>

namespace openmsx {

class Reactor;
class EventDistributor;
class CommandController;

class AfterCommand final : public Command, private EventListener
{
public:
	using Index = uint32_t; // ObjectPool<T>::Index

public:
	AfterCommand(Reactor& reactor,
	             EventDistributor& eventDistributor,
	             CommandController& commandController);
	~AfterCommand();

	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

private:
	void executeMatches(std::predicate<Index> auto pred);
	void executeSimpleEvents(EventType type);
	void afterSimpleEvent(std::span<const TclObject> tokens, TclObject& result, EventType type);
	void afterInputEvent(Event event,
	                   std::span<const TclObject> tokens, TclObject& result);
	void afterTclTime (int ms,
	                   std::span<const TclObject> tokens, TclObject& result);
	void afterTime    (std::span<const TclObject> tokens, TclObject& result);
	void afterRealTime(std::span<const TclObject> tokens, TclObject& result);
	void afterIdle    (std::span<const TclObject> tokens, TclObject& result);
	void afterInfo    (std::span<const TclObject> tokens, TclObject& result) const;
	void afterCancel  (std::span<const TclObject> tokens, TclObject& result);

	// EventListener
	int signalEvent(const Event& event) override;

private:
	std::vector<Index> afterCmds;
	Reactor& reactor;
	EventDistributor& eventDistributor;

	friend class AfterCmd;
	friend class AfterTimedCmd;
	friend class AfterRealTimeCmd;
};

} // namespace openmsx

#endif
