// $Id$

#ifndef __DISKIMAGEMANAGER_HH__
#define __DISKIMAGEMANAGER_HH__

#include <string>
#include <map>
#include "Command.hh"

// forward declarations
class FDCBackEnd;


class DiskImageManager
{
	class Drive : public Command
	{
		public:
			Drive(const std::string &driveName);
			virtual ~Drive();

			FDCBackEnd* getBackEnd();
			
		private:
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help   (const std::vector<std::string> &tokens);
			virtual void tabCompletion(std::vector<std::string> &tokens);
			
			std::string name;
			FDCBackEnd* backEnd;
	};
	
	public:
		static DiskImageManager* instance();

		/**
		 * (Un)register a drive
		 */
		void   registerDrive(const std::string &driveName);
		void unregisterDrive(const std::string &driveName);

		/**
		 * Returns the FDCBackEnd for the given drive
		 */
		FDCBackEnd* getBackEnd(const std::string &driveName);
		
		
	private:
		static DiskImageManager* oneInstance;
		std::map <const std::string, Drive*> drives;
};

#endif
