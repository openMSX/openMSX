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


FileManipulator::DiskImages::const_iterator FileManipulator::executeHelper(std::string diskname, std::string& result)
{
	DiskImages::const_iterator it = diskImages.find(diskname);
	if ( it == diskImages.end() ) {
		result += "No \'"  + diskname + "\' known";
	};
	return it;
}

string FileManipulator::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");
	} else if ( (tokens.size() != 4 && ( tokens[1] == "import"
					|| tokens[1] == "savedsk"
					|| tokens[1] == "mkdir"
					|| tokens[1] == "chdir"
					|| tokens[1] == "usePartition"
					|| tokens[1] == "export" ) )
		|| (tokens.size() != 3 && ( tokens[1] == "dir"
					|| tokens[1] == "format"
					|| tokens[1] == "useFile" ) ) ) {
		throw CommandException("Incorrect number of parameters");
	} else if (tokens[1] == "import" || tokens[1] == "export" ) {
		if (! FileOperations::isDirectory(tokens[3])) {
			throw CommandException(tokens[3] + " is not a directory");
		}
		DiskImages::const_iterator it = executeHelper(tokens[2],result);
		if (it != diskImages.end()) {
			if (tokens[1] == "export" ) exprt(it->second, tokens[3]);
			if (tokens[1] == "import" ) import(it->second, tokens[3]);
		}
	} else if (tokens[1] == "savedsk") {
		DiskImages::const_iterator it = executeHelper(tokens[2],result);
		if (it != diskImages.end()) {
			savedsk(it->second, tokens[3]);
		}
	} else if (tokens[1] == "usePartition") {
		int partition=strtol(tokens[3].c_str(),NULL,10);
		DiskImages::const_iterator it = executeHelper(tokens[2],result);
		if (it != diskImages.end()) {
			it->second->partition = partition;
			result += "New partion used : " + StringOp::toString(partition);
		}
	} else if (tokens[1] == "chdir") {
		DiskImages::const_iterator it = executeHelper(tokens[2],result);
		if (it != diskImages.end()) {
			if (chdir(it->second, tokens[3])) {
				//TODO clean-up this temp hack, used to enable relative paths
				if (tokens[3].find_first_of("/\\")==0){
					it->second->workingDir=tokens[3];
				} else {
					it->second->workingDir+="/"+tokens[3];
				}
				result += "new workingDir "+it->second->workingDir;
			} else {
				result += "chdir to "+tokens[3]+" failed!";
			}
		}
	} else if (tokens[1] == "mkdir") {
		DiskImages::const_iterator it = executeHelper(tokens[2],result);
		if (it != diskImages.end()) {
			if (! mkdir(it->second, tokens[3])) {
				result += "mkdir "+tokens[3]+" failed!";
			}
		}
	} else if (tokens[1] == "create") {
		if (tokens.size() <= 3) {
			throw CommandException("Incorrect number of parameters");
		} else {
			create(tokens);
		}
	} else if (tokens[1] == "useFile") {
		usefile(tokens[2]);
	} else if (tokens[1] == "format") {
		DiskImages::const_iterator it = executeHelper(tokens[2],result);
		if (it != diskImages.end()) {
			format(it->second );
		} else {
			result += "No \'"+tokens[2]+"\' known";
		}
	} else if (tokens[1] == "dir") {
		DiskImages::const_iterator it = diskImages.find(tokens[2]);
		if (it != diskImages.end()) {
			result += dir(it->second );
		} else {
			result += "No \'"+tokens[2]+"\' known";
		}
	} else {
		throw CommandException("Unknown subcommand: " + tokens[1]);
	}
	return result;
}

string FileManipulator::help(const vector<string>& /*tokens*/) const
{
	return
	    "filemanipulator create <filename> <size> [<size>...] : create formatted dsk-file with given (partition)size(s)\n"
	    "filemanipulator useFile <filename>             : allow manipulation of <filename>\n"
	    "filemanipulator savedsk <drivename> <filename> : save drivename as dsk-file\n"
	    "filemanipulator chdir <drivename> <dirname>    : change directory on <drivename>\n"
	    "filemanipulator mkdir <drivename> <dirname>    : create directory on <drivename>\n"
	    "filemanipulator import <drivename> <dirname>   : import all files and subdirs from <dir>\n"
	    "filemanipulator export <drivename> <dirname>   : extract all files to <dir>\n"
	    "filemanipulator dir <drivename>                : long format dir of current directory";
}

void FileManipulator::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("import");
		cmds.insert("export");
		cmds.insert("savedsk");
		cmds.insert("usePartition");
		cmds.insert("dir");
		cmds.insert("create");
		cmds.insert("useFile");
		cmds.insert("format");
		cmds.insert("chdir");
		cmds.insert("mkdir");
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
	// for conveniance purpose we run a 'filemanipulator useFile' so that this image can immediately be manipulated :-)
	usefile(tokens[2]);
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
	driveData->workingDir = std::string("\\");
}


std::string FileManipulator::dir(DriveSettings* driveData)
{
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	MSXtar workhorse(disk);
	workhorse.usePartition(driveData->partition);
	workhorse.chdir(driveData->workingDir);
	return workhorse.dir();
}

bool FileManipulator::chdir(DriveSettings* driveData, const string& filename)
{
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	MSXtar workhorse(disk);
	workhorse.usePartition(driveData->partition);
	workhorse.chdir(driveData->workingDir);
	return workhorse.chdir(filename);
}

bool FileManipulator::mkdir(DriveSettings* driveData, const string& filename)
{
	SectorAccessibleDisk* disk = &driveData->drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	MSXtar workhorse(disk);
	workhorse.usePartition(driveData->partition);
	workhorse.chdir(driveData->workingDir);
	return workhorse.mkdir(filename);
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
	workhorse.chdir(driveData->workingDir);
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
	workhorse.chdir(driveData->workingDir);
	workhorse.getDir(dirname);
}

} // namespace openmsx
