// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <vector>
#include <memory>
#include "Command.hh"
#include "SettingListener.hh"
#include "EventListener.hh"

using std::vector;
using std::auto_ptr;

namespace openmsx {

class Leds;
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
	void addDevice(auto_ptr<MSXDevice> device);
	
	void createDevices(const XMLElement& elem);
	void reInitMSX();

	// SettingListener
	virtual void update(const SettingLeafNode* setting);

	// EventListener
	bool signalEvent(const Event& event);

	void unpause();
	void pause();
	void powerOn();
	void powerOff();

	typedef vector<MSXDevice*> Devices;
	Devices availableDevices;

	bool paused;
	bool powered;
	bool needReset;

	int blockedCounter;
	bool emulationRunning;
	
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;
	Leds& leds;

	class QuitCommand : public SimpleCommand {
	public:
		QuitCommand(MSXMotherBoard& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		MSXMotherBoard& parent;
	} quitCommand;

	class ResetCmd : public SimpleCommand {
	public:
		ResetCmd(MSXMotherBoard& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		MSXMotherBoard& parent;
	} resetCommand;
};

} // namespace openmsx

#endif //__MSXMOTHERBOARD_HH__
