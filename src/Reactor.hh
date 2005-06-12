// $Id$

#ifndef REACTOR_HH
#define REACTOR_HH

#include <vector>
#include "Command.hh"
#include "SettingListener.hh"
#include "EventListener.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

class CliComm;
class BooleanSetting;

class Reactor : private SettingListener, private EventListener
{
public:
	Reactor();
	virtual ~Reactor();

	/**
	 * This starts the Scheduler.
	 */
	void run(bool powerOn);

	void block();
	void unblock();

private:

	// SettingListener
	virtual void update(const Setting* setting);

	// EventListener
	bool signalEvent(const Event& event);

	void unpause();
	void pause();
	void powerOn();
	void powerOff();

	bool paused;
	bool powered;
	bool needReset;
	bool needPowerDown;
	bool needPowerUp;

	int blockedCounter;
	bool emulationRunning;

	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;
	CliComm& output;

	MSXMotherBoard motherboard;

	class QuitCommand : public SimpleCommand {
	public:
		QuitCommand(Reactor& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Reactor& parent;
	} quitCommand;

	class ResetCmd : public SimpleCommand {
	public:
		ResetCmd(Reactor& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Reactor& parent;
	} resetCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
