// $Id$

#include "DiskImageManager.hh"
#include "CommandController.hh"
#include "FDCDummyBackEnd.hh"
#include "FDC_DSK.hh"
#include "FDC_XSA.hh"


DiskImageManager* DiskImageManager::instance()
{
	static DiskImageManager* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new DiskImageManager();
	return oneInstance;
}


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
	backEnd = NULL;
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->
			getConfigById("Media");
		std::string disk = config->getParameter(driveName);
		insertDisk(disk);
	} catch (MSXException &e) {
		// nothing specified or file not found
		ejectDisk();
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


void DiskImageManager::Drive::insertDisk(const std::string &disk)
{
	FDCBackEnd* tmp;
	try {
		// first try XSA
		tmp = new FDC_XSA(disk);
	} catch (MSXException &e) {
		// if that fails use DSK
		tmp = new FDC_DSK(disk);
	}
	delete backEnd;
	backEnd = tmp;
}

void DiskImageManager::Drive::ejectDisk()
{
	delete backEnd;
	backEnd = new FDCDummyBackEnd();
}


void DiskImageManager::Drive::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size() != 2)
		throw CommandException("Syntax error");
	if (tokens[1] == "eject") {
		ejectDisk();
	} else {
		try {
			insertDisk(tokens[1]);
		} catch (MSXException &e) {
			throw CommandException(e.desc);
		}
	}
}

void DiskImageManager::Drive::help(const std::vector<std::string> &tokens)
{
	print(name + " eject      : remove disk from virtual drive");
	print(name + " <filename> : change the disk file");
}

void DiskImageManager::Drive::tabCompletion(std::vector<std::string> &tokens)
{
	if (tokens.size() == 2)
		CommandController::completeFileName(tokens);
}
