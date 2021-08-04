#ifndef RECORDEDCOMMAND_HH
#define RECORDEDCOMMAND_HH

#include "Command.hh"
#include "StateChangeListener.hh"
#include "StateChange.hh"
#include "TclObject.hh"
#include "EmuTime.hh"
#include "dynarray.hh"

namespace openmsx {

class CommandController;
class StateChangeDistributor;
class Scheduler;

/** Commands that directly influence the MSX state should send and events
  * so that they can be recorded by the event recorder. This class helps to
  * implement that.
  *
  * Note: when a recorded command is later replayed, it's important to check
  * whether it's actually a recorded command and not some arbitrary other
  * command that someone might have inserted in a replay file. IOW when you
  * load a replay file from an untrusted source, it should never be able to
  * cause any harm. Blindly executing any Tcl command would be bad.
  */
class RecordedCommand : public Command, private StateChangeListener
{
public:
	/** This is like the execute() method of the Command class, it only
	  * has an extra time parameter.
	  */
	virtual void execute(
		span<const TclObject> tokens, TclObject& result,
		EmuTime::param time) = 0;

	/** It's possible that in some cases the command doesn't need to be
	  * recorded after all (e.g. a query subcommand). In that case you can
	  * override this method. Return false iff the command doesn't need
	  * to be recorded.
	  */
	[[nodiscard]] virtual bool needRecord(span<const TclObject> tokens) const;

protected:
	RecordedCommand(CommandController& commandController,
	                StateChangeDistributor& stateChangeDistributor,
	                Scheduler& scheduler,
	                std::string_view name);
	~RecordedCommand();

private:
	// Command
	void execute(span<const TclObject> tokens, TclObject& result) override;

	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	StateChangeDistributor& stateChangeDistributor;
	Scheduler& scheduler;
	TclObject dummyResultObject;
	TclObject* currentResultObject;
};

} // namespace openmsx

#endif
