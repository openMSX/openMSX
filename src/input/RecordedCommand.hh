// $Id: $

#ifndef RECORDEDCOMMAND_HH
#define RECORDEDCOMMAND_HH

#include "Command.hh"
#include "MSXEventListener.hh"

namespace openmsx {

class MSXEventDistributor;
class Scheduler;
class EmuTime;

/** Commands that directly influence the MSX state should send and events
  * so that they can be recorded by the event recorder. This class helps to
  * implement that.
  */
class RecordedCommand : public SimpleCommand, private MSXEventListener
{
public:
	/** This is like the execute() method of SimpleCommand
	  * @param tokens The command tokens (see SimpleCommand)
	  * @param time The current time
	  * @result Result string of the command
	  */
	virtual std::string execute(
		const std::vector<std::string>& tokens, const EmuTime& time) = 0;

	/** It's possible that in some cases the command doesn't need to be
	  * recorded after all (e.g. a query subcommand). In that case you can
	  * override this method. Return false iff the command doesn't need
	  * to be recorded. The default implementation always returns true.
	  */
	virtual bool needRecord(const std::vector<std::string>& tokens) const;

protected:
	RecordedCommand(CommandController& commandController,
	                MSXEventDistributor& msxEventDistributor,
	                Scheduler& scheduler,
	                const std::string& name);
	virtual ~RecordedCommand();

private:
	// SimpleCommand
	virtual std::string execute(const std::vector<std::string>& tokens);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	MSXEventDistributor& msxEventDistributor;
	Scheduler& scheduler;
	std::string resultString;
};

} // namespace openmsx

#endif
