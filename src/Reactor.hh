// $Id$

#ifndef REACTOR_HH
#define REACTOR_HH

#include "Command.hh"
#include "SettingListener.hh"
#include "EventListener.hh"
#include "MSXMotherBoard.hh"
#include <memory>
#include <vector>

namespace openmsx {

class CliComm;
class BooleanSetting;

/**
 * Contains the main loop of openMSX.
 * openMSX is almost single threaded: the main thread does most of the work,
 * we create additional threads only if we need blocking calls for
 * communicating with peripherals.
 * This class serializes all incoming requests so they can be handled by the
 * main thread.
 */
class Reactor : private SettingListener, private EventListener
{
public:
	Reactor();
	virtual ~Reactor();

	/**
	 * Main loop.
	 * @param autoRun Iff true, start emulation immediately.
	 */
	void run(bool autoRun);

private:

	// SettingListener
	virtual void update(const Setting* setting);

	// EventListener
	virtual bool signalEvent(const Event& event);

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

	BooleanSetting& pauseSetting;
	CliComm& output;

	std::auto_ptr<MSXMotherBoard> motherboard;

	class QuitCommand : public SimpleCommand {
	public:
		QuitCommand(Reactor& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Reactor& parent;
	} quitCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
