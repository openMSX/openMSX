// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <vector>
#include <memory>
#include "Command.hh"
#include "SettingListener.hh"
#include "EventListener.hh"

namespace openmsx {

class CliCommOutput;
class MSXDevice;
class BooleanSetting;
class XMLElement;

class MSXMotherBoard : private SettingListener, private EventListener
{
public:
	MSXMotherBoard();
	virtual ~MSXMotherBoard();

	/**
	 * This starts the Scheduler.
	 */
	void run(bool powerOn);

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void resetMSX();

	void block() { ++blockedCounter; }
	void unblock() { --blockedCounter; }

private:
	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 * This method should only be called at start-up
	 */
	void addDevice(std::auto_ptr<MSXDevice> device);
	
	void createDevices(const XMLElement& elem);
	void powerDownMSX();
	void powerUpMSX();

	// SettingListener
	virtual void update(const Setting* setting);

	// EventListener
	bool signalEvent(const Event& event);

	void unpause();
	void pause();
	void powerOn();
	void powerOff();

	typedef std::vector<MSXDevice*> Devices;
	Devices availableDevices;

	bool paused;
	bool powered;
	bool needReset;
	bool needPowerDown;
	bool needPowerUp;

	int blockedCounter;
	bool emulationRunning;
	
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;
	CliCommOutput& output;

	class QuitCommand : public SimpleCommand {
	public:
		QuitCommand(MSXMotherBoard& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		MSXMotherBoard& parent;
	} quitCommand;

	class ResetCmd : public SimpleCommand {
	public:
		ResetCmd(MSXMotherBoard& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		MSXMotherBoard& parent;
	} resetCommand;
};

} // namespace openmsx

#endif //__MSXMOTHERBOARD_HH__
