// $Id$

#include "DiskImageManager.hh"
#include "CommandController.hh"
#include "FDCDummyBackEnd.hh"
#include "FDC_DSK.hh"


DiskImageManager* DiskImageManager::instance()
{
	if (oneInstance == NULL)
		oneInstance = new DiskImageManager();
	return oneInstance;
}
DiskImageManager* DiskImageManager::oneInstance = NULL;


void DiskImageManager::registerDrive(const std::string &driveName)
{
	drives[driveName] = new Drive(driveName);
}

void DiskImageManager::unregisterDrive(const std::string &driveName)
{
	Drive* tmp = drives[driveName];
	drives.erase(driveName);
	delete tmp;
}

FDCBackEnd* DiskImageManager::getBackEnd(const std::string &driveName)
{
	return drives[driveName]->getBackEnd();
}



DiskImageManager::Drive::Drive(const std::string &driveName)
{
	name = driveName;
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->
			getConfigById("Media");
		std::string disk = config->getParameter(driveName);
		backEnd = new FDC_DSK(disk);
	} catch (MSXException &e) {
		// nothing specified or file not found
		backEnd = new FDCDummyBackEnd();
	}
	CommandController::instance()->registerCommand(*this, name);
}

DiskImageManager::Drive::~Drive()
{
	CommandController::instance()->unregisterCommand(name);
	delete backEnd;
}

FDCBackEnd* DiskImageManager::Drive::getBackEnd()
{
	return backEnd;
}

void DiskImageManager::Drive::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size() != 2)
		throw CommandException("Syntax error");
	if (tokens[1] == "eject") {
		print("Disk ejected");
		delete backEnd;
		backEnd = new FDCDummyBackEnd();
	} else {
		print("Changing disk");
		try {
			// TODO other backends
			FDCBackEnd* tmp = new FDC_DSK(tokens[1]);
			delete backEnd;
			backEnd = tmp;
		} catch (MSXException &e) {
			throw CommandException(e.desc);
		}
	}
}

void DiskImageManager::Drive::help(const std::vector<std::string> &tokens)
{
	// TODO use correct name
	print("disk eject      : remove disk from virtual drive");
	print("disk <filename> : change the disk file");
}

void DiskImageManager::Drive::tabCompletion(std::vector<std::string> &tokens)
{
	if (tokens.size() == 2)
		CommandController::completeFileName(tokens);
}
