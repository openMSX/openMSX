// $Id$

#include "DiskImageManager.hh"
#include "CommandController.hh"
#include "FDCDummyBackEnd.hh"
#include "FDC_DSK.hh"
#include "FDC_XSA.hh"
#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"


DiskImageCLI diskImageCLI;

DiskImageCLI::DiskImageCLI()
{
	CommandLineParser::instance()->registerOption("-diska", this);
	CommandLineParser::instance()->registerOption("-diskb", this);

	CommandLineParser::instance()->registerFileType("dsk", this);
	CommandLineParser::instance()->registerFileType("di1", this);
	CommandLineParser::instance()->registerFileType("di2", this);
	CommandLineParser::instance()->registerFileType("xsa", this);

	driveLetter = 'a';
}

void DiskImageCLI::parseOption(const std::string &option,
                         std::list<std::string> &cmdLine)
{
	driveLetter = option[5];	// -disk_
	std::string filename = cmdLine.front();
	cmdLine.pop_front();

	parseFileType(filename);
}
const std::string& DiskImageCLI::optionHelp()
{
	static const std::string text("TODO");
	return text;
}

void DiskImageCLI::parseFileType(const std::string &filename_)
{
	std::string filename(filename_); XML::Escape(filename);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<config id=\"disk" << driveLetter << "\">";
	s << "<parameter name=\"filename\">" << filename << "</parameter>";
	s << "</config>";
	s << "</msxconfig>";
	
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadStream(s);
	driveLetter++;
}
const std::string& DiskImageCLI::fileTypeHelp()
{
	static const std::string text("TODO");
	return text;
}


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
			getConfigById(driveName);
		std::string disk = config->getParameter("filename");
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
