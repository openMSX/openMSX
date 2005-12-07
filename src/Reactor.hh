// $Id$

#ifndef REACTOR_HH
#define REACTOR_HH

#include "Command.hh"
#include "Observer.hh"
#include "EventListener.hh"

namespace openmsx {

class CliComm;
class BooleanSetting;
class MSXMotherBoard;
class Setting;
class CommandLineParser;

/**
 * Contains the main loop of openMSX.
 * openMSX is almost single threaded: the main thread does most of the work,
 * we create additional threads only if we need blocking calls for
 * communicating with peripherals.
 * This class serializes all incoming requests so they can be handled by the
 * main thread.
 */
class Reactor : private Observer<Setting>, private EventListener
{
public:
	explicit Reactor(MSXMotherBoard& motherBoard);
	~Reactor();

	/**
	 * Main loop.
	 * @param autoRun Iff true, start emulation immediately.
	 */
	void run(CommandLineParser& parser);

private:
	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual void signalEvent(const Event& event);

	void block();
	void unblock();

	void unpause();
	void pause();

	bool paused;

	int blockedCounter;

	/**
	 * True iff the Reactor should keep running.
	 * When this is set to false, the Reactor will end the main loop after
	 * finishing the pending request(s).
	 */
	bool running;

	MSXMotherBoard& motherBoard;
	BooleanSetting& pauseSetting;
	CliComm& cliComm;

	class QuitCommand : public SimpleCommand {
	public:
		QuitCommand(CommandController& commandController, Reactor& reactor);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Reactor& reactor;
	} quitCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
