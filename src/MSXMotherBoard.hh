// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <list>
#include "MSXConfig.hh"
#include "Command.hh"
#include "MSXCPUInterface.hh"
#include "Schedulable.hh"

// forward declarations
class MSXDevice;
class EmuTime;


class MSXMotherBoard : public MSXCPUInterface, public Schedulable
{
	public:
		/**
		 * Destructor
		 */
		virtual ~MSXMotherBoard();

		/**
		 * this class is a singleton class
		 * usage: MSXMotherBoard::instance()->method(args);
		 */
		static MSXMotherBoard *instance();

		/**
		 * All MSXDevices should be registered by the MotherBoard.
		 * This method should only be called at start-up
		 */
		void addDevice(MSXDevice *device);
		
		/**
		 * To remove a device completely from configuration
		 * fe. yanking a cartridge out of the msx
		 *
		 * TODO this method not yet used!!
		 */
		void removeDevice(MSXDevice *device);

		/**
		 * This starts the Scheduler.
		 */
		void startMSX();

		/**
		 * This will reset all MSXDevices (the reset() method of
		 * all registered MSXDevices is called)
		 */
		void resetMSX(const EmuTime &time);

		/**
		 * This will destroy all MSXDevices (the destructor of
		 * all registered MSXDevices is called)
		 */
		void destroyMSX();


	private:
		MSXMotherBoard(MSXConfig::Config *config);
		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		
		class ResetCmd : public Command {
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help   (const std::vector<std::string> &tokens);
		};
		ResetCmd resetCmd;

		std::list<MSXDevice*> availableDevices;
};
#endif //__MSXMOTHERBOARD_HH__
