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

class AfterCommand final : public Command, private EventListener
{
public:
	using Index = uint32_t; // ObjectPool<T>::Index

public:
	AfterCommand(Reactor& reactor,
	             EventDistributor& eventDistributor,
	             CommandController& commandController);
	~AfterCommand();

	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

private:
	template<typename PRED> void executeMatches(PRED pred);
	void executeSimpleEvents(EventType type);
	void afterSimpleEvent(span<const TclObject> tokens, TclObject& result, EventType type);
	void afterInputEvent(const Event& event,
	                   span<const TclObject> tokens, TclObject& result);
	void afterTclTime (int ms,
	                   span<const TclObject> tokens, TclObject& result);
	void afterTime    (span<const TclObject> tokens, TclObject& result);
	void afterRealTime(span<const TclObject> tokens, TclObject& result);
	void afterIdle    (span<const TclObject> tokens, TclObject& result);
	void afterInfo    (span<const TclObject> tokens, TclObject& result);
	void afterCancel  (span<const TclObject> tokens, TclObject& result);

	// EventListener
	int signalEvent(const Event& event) noexcept override;

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
