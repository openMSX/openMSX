// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <list>
#include "Command.hh"
#include "Thread.hh"

using std::list;

namespace openmsx {

class MSXDevice;


class MSXMotherBoard
{
public:
	/**
	 * this class is a singleton class
	 * usage: MSXMotherBoard::instance()->method(args);
	 */
	static MSXMotherBoard* instance();

	/**
	 * This starts the Scheduler.
	 */
	void run(bool powerOn);

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void resetMSX();
	void reInitMSX();

private:
	MSXMotherBoard();
	~MSXMotherBoard();

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

	class ResetCmd : public Command {
		virtual string execute(const vector<string>& tokens) throw();
		virtual string help(const vector<string>& tokens) const throw();
	} resetCmd;

	list<MSXDevice*> availableDevices;
};

} // namespace openmsx
#endif //__MSXMOTHERBOARD_HH__
