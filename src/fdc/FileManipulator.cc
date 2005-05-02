// $Id$

#include "FileManipulator.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "SectorBasedDisk.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "DiskDrive.hh"
#include "MSXtar.hh"
#include <cassert>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

FileManipulator::FileManipulator()
{
	CommandController::instance().registerCommand(this, "filemanipulator");
}

FileManipulator::~FileManipulator()
{
	if ( diskImages.find(std::string("imagefile")) != diskImages.end()) {
		unregisterDrive(*(diskImages[std::string("imagefile")]->drive) , std::string("imagefile"));
	};
	CommandController::instance().unregisterCommand(this, "filemanipulator");
}

FileManipulator& FileManipulator::instance()
{
	static FileManipulator oneInstance;
	return oneInstance;
}

//Disk& FileManipulator::getDisk() const
//{
// return &this;
//}

//void FileManipulator::readLogicalSector(unsigned sector, byte* buf)
//{
//  
//}
//void FileManipulator::writeLogicalSector(unsigned sector, const byte* buf)
//{
//}
//unsigned FileManipulator::getNbSectors() const
//{
//  return 1440; 
//}

void FileManipulator::registerDrive(DiskContainer& drive, const string& imageName)
{
	assert(diskImages.find(imageName) == diskImages.end());
	diskImages[imageName] = new DriveSettings();
	diskImages[imageName]->drive = &drive;
	diskImages[imageName]->partition = 0;
	diskImages[imageName]->workingDir = std::string("\\");
}

void FileManipulator::unregisterDrive(DiskContainer& drive, const string& imageName)
{
	if (&drive); // avoid warning
	assert(diskImages.find(imageName) != diskImages.end());
	assert(diskImages[imageName]->drive == &drive);
	delete diskImages[imageName];
	diskImages.erase(imageName);
}


string FileManipulator::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");
	} else if (tokens[1] == "import" || tokens[1] == "export" ) {
		if (tokens.size() != 4) {
			throw CommandException("Incorrect number of parameters");
		} else {
		  	if (! FileOperations::isDirectory(tokens[3])) {
				throw CommandException(tokens[3] + " is not a directory");
			}
			DiskImages::const_iterator it = diskImages.find(tokens[2]);
			if (it != diskImages.end()) {
				if (tokens[1] == "export" ) exprt(it->second, tokens[3]);
				if (tokens[1] == "import" ) import(it->second, tokens[3]);
			}
		}
	} else if (tokens[1] == "savedsk") {
		if (tokens.size() != 4) {
			throw CommandException("Incorrect number of parameters");
		} else {
			DiskImages::const_iterator it = diskImages.find(tokens[2]);
			if (it != diskImages.end()) {
				savedsk(it->second, tokens[3]);
			}
		}
	} else if (tokens[1] == "export") {
		if (tokens.size() <= 3) {
			throw CommandException("Incorrect number of parameters");
		} else {
			throw CommandException("Not implemented yet");
		}
	} else if (tokens[1] == "import") {
		if (tokens.size() <= 3) {
			throw CommandException("Incorrect number of parameters");
		} else {
			throw CommandException("Not implemented yet");
		}
	} else if (tokens[1] == "usePartition") {
		if (tokens.size() <= 3) {
			throw CommandException("Incorrect number of parameters");
		} else {
			int partition=strtol(tokens[3].c_str(),NULL,10);
			DiskImages::const_iterator it = diskImages.find(tokens[2]);
			if (it != diskImages.end()) {
				it->second->partition = partition;
				result += "New partion used : " + StringOp::toString(partition);
			}
		}
	} else if (tokens[1] == "create") {
		if (tokens.size() <= 3) {
			throw CommandException("Incorrect number of parameters");
		} else {
			create(tokens);
		}
	} else if (tokens[1] == "useFile") {
		if (tokens.size() <= 2) {
			throw CommandException("Incorrect number of parameters");
		} else {
			usefile(tokens[2]);
		}
	} else if (tokens[1] == "format") {
		if (tokens.size() <= 2) {
			throw CommandException("Incorrect number of parameters");
		} else {
			DiskImages::const_iterator it = diskImages.find(tokens[2]);
			if (it != diskImages.end()) {
				format(it->second );
			}
		}
	} else {
		throw CommandException("Unknown subcommand: " + tokens[1]);
	}
	return result;
}

string FileManipulator::help(const vector<string>& /*tokens*/) const
{
	return
	    "filemanipulator savedsk <drivename> <filename> : save drivename as dsk-file\n"
	    "filemanipulator import <drivename> <dirname>   : import all files and subdirs from <dir>\n"
	    "filemanipulator export <drivename> <dirname>   : extract all files to <dir>\n"
	    "filemanipulator msxtree <drivename>            : show the dir+files structure\n"
	    "filemanipulator msxdir <drivename> <filename>  : long format dir of <filename>";
}

void FileManipulator::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("import");
		cmds.insert("export");
		cmds.insert("savedsk");
		cmds.insert("usePartition");
		//cmds.insert("msxtree");
		//cmds.insert("msxdir");
		cmds.insert("create");
		cmds.insert("useFile");
		cmds.insert("format");
		CommandController::completeString(tokens, cmds);
	} else if (tokens.size() == 4 && tokens[1] == "usePartition") {
		int partition=0;
		DiskImages::const_iterator it = diskImages.find(tokens[2]);
		if (it != diskImages.end()) {
			partition=it->second->partition;
		}
		set<string> cmds;//TODO FIX this since I suppose that this can be simplified
		cmds.insert(StringOp::toString(partition)); 
		CommandController::completeString(tokens, cmds);
	} else if (tokens.size() == 2 && (  tokens[1] == "create" || tokens[1] == "useFile") ) {
			CommandController::completeFileName(tokens);
	} else if (tokens.size() == 3) {
		set<string> names;
		for (DiskImages::const_iterator it = diskImages.begin();
		     it != diskImages.end(); ++it) {
			names.insert(it->first);
		}
		CommandController::completeString(tokens, names);
	} else if (tokens.size() >= 4) {
		if ((tokens[1] == "savedsk") ||
		    (tokens[1] == "import")  ||
		    (tokens[1] == "export")) {
			CommandController::completeFileName(tokens);
		} else if ( tokens[1] == "create") {
			set<string> cmds;
			cmds.insert("-dos1");
			cmds.insert("-dos2");
			cmds.insert("1440");
			cmds.insert("720");
			CommandController::completeString(tokens, cmds);
		}
	}
}

void FileManipulator::savedsk(DriveSettings* driveData, const string& filename)
{
	//SectorAccessibleDisk* disk = dynamic_cast<SectorAccessibleDisk*>(&drive->getDisk());
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	unsigned nrsectors = disk->getNbSectors();
	byte buf[SectorBasedDisk::SECTOR_SIZE];
	File file(filename,CREATE);
	for (unsigned i = 0; i < nrsectors; ++i) {
		disk->readLogicalSector(i, buf);
		file.write(buf, SectorBasedDisk::SECTOR_SIZE);
	}
}

void FileManipulator::usefile(const string& filename)
{
	const std::string imageName=std::string("imagefile");
	// unregister imagefile if already using one.
	if ( diskImages.find(imageName) != diskImages.end()) {
		delete dynamic_cast<FileDriveCombo*>(diskImages[imageName]->drive) ;
		unregisterDrive(*(diskImages[imageName]->drive) , std::string("imagefile"));
	};

	registerDrive( *(new FileDriveCombo(filename)) , imageName );
}

void FileManipulator::create(const std::vector<std::string>& tokens)
{
	//iterate over tokens and split into options or size specifications
	// also alter all size specifications into sector based sizes
	vector<string> options;
	vector<int> sizes;
	CliComm::instance().printWarning("tokens.size() => "+StringOp::toString(tokens.size()) );
	for (unsigned int i=3 ; i<tokens.size() ; ++i ){
		CliComm::instance().printWarning("tokens["+StringOp::toString(i) +"] => "+tokens[i]);
		if ( (tokens[i].c_str())[0] == '-' ){
			options.push_back(tokens[i]);
		} else {
			char *Q;
			int scale=512 ; //SECTOR_SIZE
			int sectors=strtol( tokens[i].c_str() , &Q , 10 );

			switch (*Q) {
			  case 'b':
			  case 'B':
			    scale=1;
			    break;
			  case 'k':
			  case 'K':
			    scale=1024;
			    break;
			  case 'm':
			  case 'M':
			    scale=1024*1024;
			    break;
			  case 's':
			  case 'S':
			    scale=512 ; //SECTOR_SIZE;
			    break;
			}
			sectors = (sectors*scale)/512 ; //SECTOR_SIZE;
			sizes.push_back(sectors);
		}
	}
	MSXtar workhorse(NULL);
	workhorse.createDiskFile(tokens[2],sizes,options);
	// here we call MSXtar to create the file
	// then we run a (to be implemented) 'usefile' filemanipulator
}

void FileManipulator::format(DriveSettings* driveData)
{
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	MSXtar workhorse(disk);
	workhorse.usePartition(driveData->partition);
	workhorse.format();
}


void FileManipulator::import(DriveSettings* driveData, const string& filename)
{
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	MSXtar workhorse(disk);
	workhorse.usePartition(driveData->partition);
	// since format is a command now, this is up to the user workhorse.format();
	workhorse.addDir(filename);
}

void FileManipulator::exprt(DriveSettings* driveData, const string& dirname)
{
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	MSXtar workhorse(disk);
	workhorse.usePartition(driveData->partition);
	workhorse.getDir(dirname);
}

} // namespace openmsx
