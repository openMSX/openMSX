// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <list>
#include "SettingListener.hh"
#include "Command.hh"
#include "Thread.hh"

using std::list;

namespace openmsx {

class MSXDevice;
class BooleanSetting;

class MSXMotherBoard : private SettingListener
{
public:
	MSXMotherBoard();
	virtual ~MSXMotherBoard();

	/**
	 * This starts the Scheduler.
	 */
	void run(bool powerOn);

private:
	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 * This method should only be called at start-up
	 */
	void addDevice(MSXDevice* device);
	
	/**
	 * To remove a device completely from configuration
	 * fe. yanking a cartridge out of the msx
	 *
	 * TODO this method not yet used!!
	 */
	void removeDevice(MSXDevice* device);

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void resetMSX();
	void reInitMSX();

	// SettingListener
	virtual void update(const SettingLeafNode* setting) throw();

	class ResetCmd : public Command {
	public:
		ResetCmd(MSXMotherBoard& parent);
		virtual string execute(const vector<string>& tokens) throw();
		virtual string help(const vector<string>& tokens) const throw();
	private:
		MSXMotherBoard& parent;
	} resetCmd;

	list<MSXDevice*> availableDevices;

	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif //__MSXMOTHERBOARD_HH__
