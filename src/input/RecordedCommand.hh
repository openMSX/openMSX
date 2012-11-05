// $Id$

#ifndef RECORDEDCOMMAND_HH
#define RECORDEDCOMMAND_HH

#include "Command.hh"
#include "StateChangeListener.hh"
#include "StateChange.hh"
#include "TclObject.hh"
#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class CommandController;
class StateChangeDistributor;
class Scheduler;

/** This class is used to for Tcl commands that directly influence the MSX
  * state (e.g. plug, disk<x>, cassetteplayer, reset). It's passed via an
  * event because the recording needs to see these.
  */
class MSXCommandEvent : public StateChange
{
public:
	MSXCommandEvent() {} // for serialize
	MSXCommandEvent(const std::vector<std::string>& tokens, EmuTime::param time);
	MSXCommandEvent(const std::vector<TclObject>& tokens,  EmuTime::param time);
	virtual ~MSXCommandEvent();
	const std::vector<TclObject>& getTokens() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
private:
	std::vector<TclObject> tokens;
};


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
	  * There are two variants of this method.  The one with tclObjects is
	  * the fastest, the one with strings is often more convenient to use.
	  * Subclasses must reimplement exactly one of these two.
	  */
	virtual void execute(
		const std::vector<TclObject>& tokens, TclObject& result,
		EmuTime::param time);
	virtual std::string execute(
		const std::vector<std::string>& tokens, EmuTime::param time);

	/** It's possible that in some cases the command doesn't need to be
	  * recorded after all (e.g. a query subcommand). In that case you can
	  * override this method. Return false iff the command doesn't need
	  * to be recorded.
	  * Similar to the execute() method above there are two variants of
	  * this method. However in this case it's allowed to override none
	  * or just one of the two variants (but not both).
	  * The default implementation always returns true (will always
	  * record). If this default implementation is fine but if speed is
	  * very important (e.g. the debug command) it is still recommenced
	  * to override the TclObject variant of this method (and just return
	  * true).
	  */
	virtual bool needRecord(const std::vector<TclObject>& tokens) const;
	virtual bool needRecord(const std::vector<std::string>& tokens) const;

protected:
	RecordedCommand(CommandController& commandController,
	                StateChangeDistributor& stateChangeDistributor,
	                Scheduler& scheduler,
	                string_ref name);
	virtual ~RecordedCommand();

private:
	// Command
	virtual void execute(const std::vector<TclObject>& tokens,
	                     TclObject& result);

	// StateChangeListener
	virtual void signalStateChange(const std::shared_ptr<StateChange>& event);
	virtual void stopReplay(EmuTime::param time);

	StateChangeDistributor& stateChangeDistributor;
	Scheduler& scheduler;
	const std::unique_ptr<TclObject> dummyResultObject;
	TclObject* currentResultObject;
};

} // namespace openmsx

#endif
