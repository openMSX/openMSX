// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <fstream>
#include <vector>
#include "MSXConfig.hh"
#include "ConsoleSource/ConsoleCommand.hh"
#include "MSXCPUInterface.hh"

// forward declarations
class MSXDevice;
class EmuTime;


class MSXMotherBoard : public MSXCPUInterface
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
		 * TODO this method is unimplemented!!
		 */
		void removeDevice(MSXDevice *device);

		/**
		 * This will initialize all MSXDevices (the init() method of
		 * all registered MSXDevices is called)
		 */
		void InitMSX();

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


		/**
		 * TODO
		 */
		void restoreMSX();	// TODO unimplemented!!
		/**
		 * TODO
		 */
		void saveStateMSX(std::ofstream &savestream);

	private:
		MSXMotherBoard(MSXConfig::Config *config);
		
		class ResetCmd : public ConsoleCommand {
			virtual void execute(const char *commandLine);
			virtual void help(const char *commandLine);
		};
		ResetCmd resetCmd;

		std::vector<MSXDevice*> availableDevices;
		static MSXMotherBoard *oneInstance;
};
#endif //__MSXMOTHERBOARD_HH__
