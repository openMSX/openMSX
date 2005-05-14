// $Id$

#include "FileManipulator.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "SectorBasedDisk.hh"
#include "FileDriveCombo.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "MSXtar.hh"
#include <cassert>
#include <ctype.h>

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
	unregisterImageFile();
	assert(diskImages.empty()); // all DiskContainers must be unregistered
	CommandController::instance().unregisterCommand(this, "filemanipulator");
}

FileManipulator& FileManipulator::instance()
{
	static FileManipulator oneInstance;
	return oneInstance;
}

static const string IMAGE_FILE = "imagefile";

void FileManipulator::unregisterImageFile()
{
	if (imageFile.get()) {
		unregisterDrive(*imageFile.get(), IMAGE_FILE);
	}
}

void FileManipulator::registerDrive(DiskContainer& drive, const string& imageName)
{
	assert(diskImages.find(imageName) == diskImages.end());
	diskImages[imageName].drive = &drive;
	diskImages[imageName].defaultPartition = 0;
	diskImages[imageName].partition = 0;
	for (int i=0 ; i < 31 ; ++i) {
		diskImages[imageName].workingDir[i] = "\\";
	}
}

void FileManipulator::unregisterDrive(DiskContainer& drive, const string& imageName)
{
	if (&drive); // avoid warning
	assert(diskImages.find(imageName) != diskImages.end());
	assert(diskImages[imageName].drive == &drive);
	diskImages.erase(imageName);
}


FileManipulator::DriveSettings& FileManipulator::getDriveSettings(
	const string& diskname)
{
	// first split of the end numbers if present
	// these will be used as partition indication

	string::size_type pos = diskname.find_first_of("0123456789");
	string tmp = (pos != string::npos)
	           ? diskname.substr(0, pos )
	           : diskname;
	//CliComm::instance().printWarning("FileManipulator::getDriveSettings tmp= " + tmp);
	//CliComm::instance().printWarning("FileManipulator::getDriveSettings pos= " + StringOp::toString(pos));

	DiskImages::iterator it = diskImages.find(tmp);
	if (it == diskImages.end()) {
		throw CommandException("No \'"  + tmp + "\' known");
	}

	it->second.partition = it->second.defaultPartition ;
	if (pos != string::npos){
		//CliComm::instance().printWarning("FileManipulator::getDriveSettings partition= " + diskname.substr(pos));
		int i = strtol(diskname.substr(pos).c_str(), NULL , 10) ;
		if (i==0 || i>31) {
			throw CommandException("Invalid MSX-IDE partition specified");
		} else {
			it->second.partition = i-1;
		}
	}
	return it->second;
}

SectorAccessibleDisk& FileManipulator::getDisk(const DriveSettings& driveData)
{
	SectorAccessibleDisk* disk = driveData.drive->getDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	return *disk;
}


string FileManipulator::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");

	} else if ((tokens.size() != 4 && (tokens[1] == "import"
	                                || tokens[1] == "savedsk"
	                                || tokens[1] == "mkdir"
	                                || tokens[1] == "export"))
	        || (tokens.size() != 3 && (tokens[1] == "dir"
	                                || tokens[1] == "format"))
	        || ((tokens.size() < 3 || tokens.size() > 4) &&
		  			(tokens[1] == "chdir"
	                                || tokens[1] == "usePartition"))
	        || (tokens.size() > 3 && tokens[1] == "useFile")
	        || (tokens.size() <= 3 && (tokens[1] == "create"))) {
		throw CommandException("Incorrect number of parameters");

	} else if (tokens[1] == "import" || tokens[1] == "export" ) {
		if (!FileOperations::isDirectory(tokens[3])) {
			throw CommandException(tokens[3] + " is not a directory");
		}
		DriveSettings& settings = getDriveSettings(tokens[2]);
		if (tokens[1] == "export") exprt (settings, tokens[3]);
		if (tokens[1] == "import") import(settings, tokens[3]);

	} else if (tokens[1] == "savedsk") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		savedsk(settings, tokens[3]);

	} else if (tokens[1] == "usePartition") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		if (tokens.size() == 3){
			result += "Current default partition: "+StringOp::toString(1 + settings.defaultPartition);
		} else {
			int i= StringOp::stringToInt(tokens[3]) ;
			if (i==0 || i>31) {
				result += "Invalid partition specification,"
			    " using current default partition " + StringOp::toString(1 + settings.defaultPartition) ; 
			} else {
				result += "New default partion: " + StringOp::toString(i);
				settings.defaultPartition = i - 1 ;
			}
		}
	} else if (tokens[1] == "chdir") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		if (tokens.size() == 3){
			result += "Current directory: "+settings.workingDir[settings.partition];
		} else {
			chdir(settings, tokens[3], result);
		}
	} else if (tokens[1] == "mkdir") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		mkdir(settings, tokens[3]);

	} else if (tokens[1] == "create") {
		create(tokens);

	} else if (tokens[1] == "useFile") {
		if (tokens.size() == 3){
			usefile(tokens[2]);
		} else {
			result += "Current file: "+fileInUse;
		}

	} else if (tokens[1] == "format") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		format(settings);

	} else if (tokens[1] == "dir") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		dir(settings, result);

	} else {
		throw CommandException("Unknown subcommand: " + tokens[1]);
	}
	return result;
}

string FileManipulator::help(const vector<string>& tokens) const
{
	string helptext;
	if (tokens.size() >= 2) {
	  if (tokens[1] == "import" ) {
	  helptext=
	    "filemanipulator import <drivename> <dirname>\n"
	    "Import all files and subdirs from the host OS in <dirname> into the\n"
	    "<drivename> in the current MSX subdirectory as was specified with the\n"
	    "last chdir command.\n";
	  } else if (tokens[1] == "export" ) {
	  helptext=
	    "filemanipulator export <drivename> <dirname>\n"
	    "Extract all files and subdirs from the MSX subdirectory specified with\n"
	    "the chdir command to the host OS in <dirname>.\n";
	  } else if (tokens[1] == "savedsk") {
	  helptext=
	    "filemanipulator savedsk <drivename> <filename> : save drivename as dsk-file\n"
	    "This saves the complete drive content to <filename>, it is not possible to\n"
	    "save just one partition. The main purpose of this command is to make it\n"
	    "possible to save a 'ramdsk' into a file and to take 'live backups' of\n"
	    "dsk-files in use.\n";
	  } else if (tokens[1] == "usePartition") {
	  helptext=
	    "filemanipulator usePartition <drivename> <partitionnumber>\n"
	    "If <drivename> contains an MSX IDE partition table, this command\n"
	    "changes the default partition that will be used for commands like\n"
	    "chdir, import, dir, ... in case you use these commands without\n"
	    "appending the partion number to the drivename.\n";
	  } else if (tokens[1] == "chdir") {
	  helptext=
	    "filemanipulator chdir <drivename> <dirname>\n"
	    "Change the working directory on <drivename>. This will be the\n"
	    "directory were the 'import', 'export' and 'dir' commands will\n"
	    "work on.\n"
	    "In case of a partitioned drive, each partition has its own\n"
	    "working directory.\n";
	  } else if (tokens[1] == "mkdir") {
	  helptext=
	    "filemanipulator mkdir <drivename> <dirname>\n"
	    "This creates the directory on <drivename>. If needed, all missing\n"
	    "parent directories are created at the same time. Accepts both\n"
	    "absolute and relative pathnames.\n";
	  } else if (tokens[1] == "create") {
	  helptext=
	    "filemanipulator create <filename> <size> [<size>...]\n"
	    "Creates a formatted dsk file with the given size.\n"
	    "If multiple sizes are given, a partitioned disk image will\n"
	    "be created with each partition having the size as indicated. By\n"
	    "default the sizes are expressed in kilobyte, add the postfix M\n"
	    "for megabyte.\n";
	  } else if (tokens[1] == "useFile") {
	  helptext=
	    "filemanipulator useFile <filename>\n"
	    "use this if you want to alter the content of a dsk file that isn't\n"
	    "in use by the emulated MSX. It wil create a new diskname called\n"
	    +IMAGE_FILE+" to manipulate the content of <filename>\n";
	  } else if (tokens[1] == "format") {
	  helptext=
	    "filemanipulator format <drivename>\n"
	    "formats the current (partition on) <drivename> with a regular\n"
	    "FAT12 MSX filesystem\n";
	  } else if (tokens[1] == "dir") {
	  helptext=
	    "filemanipulator dir <drivename>\n"
	    "Shows the content of the current directory on <drivename>\n";
	  } else {
	  helptext="unknown filemanipulator subcommand: "+tokens[2];
	  }
	} else {
	  helptext=string(
	    "filemanipulator create <fn> <sz> [<sz> ...] : create a formatted dsk file with name <fn>, having\n"
	    "                                              the given (partition) size(s)\n"
	    "filemanipulator useFile <filename>          : allow manipulation of <filename>\n"
	    "filemanipulator savedsk <drivename> <fn>    : save <drivename> as dsk file with name <fn>\n"
	    "filemanipulator usePartition <drv> <partnr> : change default partition for drive <drv> to <partnr>\n"
	    "filemanipulator format <drivename>          : format (a partition) on <drivename>\n"
	    "filemanipulator chdir <drivename> <dirname> : change directory on <drivename>\n"
	    "filemanipulator mkdir <drivename> <dirname> : create directory on <drivename>\n"
	    "filemanipulator dir <drivename>             : long format dir of current directory on <drivename>\n"
	    "filemanipulator import <drvname> <dirname>  : import all files and subdirs from <dirname>\n"
	    "filemanipulator export <drvname> <dirname>  : export all files on <drvname> to <dir>\n"
	    "For more info use 'help filemanipulator <subcommand>'\n");
	}
	return helptext;
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
		int partition = 0;
		DiskImages::const_iterator it = diskImages.find(tokens[2]);
		if (it != diskImages.end()) {
			partition = it->second.partition;
		}
		set<string> cmds;
		cmds.insert(StringOp::toString(partition + 1)); 
		CommandController::completeString(tokens, cmds);

	} else if (tokens.size() == 2 && (tokens[1] == "create" || tokens[1] == "useFile")) {
		CommandController::completeFileName(tokens);

	} else if (tokens.size() == 3) {
		set<string> names;
		for (DiskImages::const_iterator it = diskImages.begin();
		     it != diskImages.end(); ++it) {
			names.insert(it->first);
			// if it has partitions then we also add the partition numbers to the autocompletion
			SectorAccessibleDisk* sectorDisk = it->second.drive->getDisk();
			if (sectorDisk != NULL) {
				MSXtar workhorse(*sectorDisk);
				for (int i = 0; i < 31; ++i) {
					if (workhorse.hasPartition(i)) {
						names.insert(it->first + StringOp::toString(i + 1));
					}
				}
			}
		}
		CommandController::completeString(tokens, names);

	} else if (tokens.size() >= 4) {
		if ((tokens[1] == "savedsk") ||
		    (tokens[1] == "import")  ||
		    (tokens[1] == "export")) {
			CommandController::completeFileName(tokens);
		} else if (tokens[1] == "create") {
			set<string> cmds;
			cmds.insert("-dos1");
			cmds.insert("-dos2");
			cmds.insert("360");
			cmds.insert("720");
			cmds.insert("32M");
			CommandController::completeString(tokens, cmds);
		}
	}
}

void FileManipulator::savedsk(const DriveSettings& driveData,
                              const string& filename)
{
	SectorAccessibleDisk& disk = getDisk(driveData);
	unsigned nrsectors = disk.getNbSectors();
	byte buf[SectorBasedDisk::SECTOR_SIZE];
	File file(filename, CREATE);
	for (unsigned i = 0; i < nrsectors; ++i) {
		disk.readLogicalSector(i, buf);
		file.write(buf, SectorBasedDisk::SECTOR_SIZE);
	}
}

void FileManipulator::usefile(const string& filename)
{
	unregisterImageFile();
	if (filename != "eject") {
		fileInUse=filename;
		imageFile.reset(new FileDriveCombo(filename));
		registerDrive(*imageFile.get(), IMAGE_FILE);
	} else {
		fileInUse=std::string("");
	}
}

void FileManipulator::create(const vector<string>& tokens)
{
	vector<unsigned> sizes;
	unsigned totalSectors = 0;
	for (unsigned i = 3; i < tokens.size(); ++i) {
		char* q;
		int sectors = strtol(tokens[i].c_str(), &q, 10);
		int scale = 1024; //default now in kilobyte instead of SectorBasedDisk::SECTOR_SIZE; 
		switch (tolower(*q)) {
			case 'b':
				scale = 1;
				break;
			case 'k':
				scale = 1024;
				break;
			case 'm':
				scale = 1024 * 1024;
				break;
			case 's':
				scale = SectorBasedDisk::SECTOR_SIZE;
				break;
		}
		sectors = (sectors * scale) / SectorBasedDisk::SECTOR_SIZE;
		// for a 32MB disk or greater the sectors would be >= 65536
		// since MSX use 16 bits for this, in case of sectors = 65536 
		// the truncated word will be 0 -> formatted as 320 Kb disk!
		if (sectors > 65535) sectors = 65535; // this is the max size for fat12 :-)

		sizes.push_back(sectors);
		totalSectors += sectors;
	}
	if (sizes.size() > 1) {
		// extra sector for partition table
		++totalSectors;
	}
	
	// create file with correct size
	try {
		File file(tokens[2], CREATE);
		file.truncate(totalSectors * SectorBasedDisk::SECTOR_SIZE);
	} catch (FileException& e) {
		throw CommandException("Couldn't create image: " + e.getMessage());
	}

	// use this file 
	usefile(tokens[2]);

	// initialize (create partition tables and format partitions)
	MSXtar workhorse(*imageFile);
	workhorse.createDiskFile(sizes);
}

void FileManipulator::format(DriveSettings& driveData)
{
	MSXtar workhorse(getDisk(driveData));
	workhorse.usePartition(driveData.partition);
	workhorse.format();
	driveData.workingDir[driveData.partition] = "\\";
}

void FileManipulator::restoreCWD(MSXtar& workhorse, DriveSettings& driveData)
{
	if (!workhorse.hasPartitionTable() ) {
		workhorse.usePartition(0) ; //read the bootsector
	} else if (!workhorse.usePartition(driveData.partition)) {
		throw CommandException("Partition " + StringOp::toString(1+driveData.partition) +
		          " doesn't exist on this device. Command aborted, please retry.");
	}
	if (!workhorse.chdir(driveData.workingDir[driveData.partition])) {
		driveData.workingDir[driveData.partition] = "\\";
		throw CommandException("Directory " + driveData.workingDir[driveData.partition] +
		          " doesn't exist anymore. Went back to root "
			  "directory. Command aborted, please retry.");
	}
}

void FileManipulator::dir(DriveSettings& driveData, string& result)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);
	result += workhorse.dir();
}

void FileManipulator::chdir(DriveSettings& driveData,
                            const string& filename, string& result)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);
	if (!workhorse.chdir(filename)) {
		throw CommandException("chdir " + filename + " failed");
	}
	// TODO clean-up this temp hack, used to enable relative paths
	if (filename.find_first_of("/\\") == 0) {
		driveData.workingDir[driveData.partition] = filename;
	} else {
		driveData.workingDir[driveData.partition] += '/' + filename;
	}
	result += "New working directory: " + driveData.workingDir[driveData.partition];
}

void FileManipulator::mkdir(DriveSettings& driveData, const string& filename)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);
	if (!workhorse.mkdir(filename)) {
		throw CommandException ("mkdir " + filename + " failed");
	}
}

void FileManipulator::import(DriveSettings& driveData, const string& filename)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);
	workhorse.addDir(filename); // TODO can this fail?
}

void FileManipulator::exprt(DriveSettings& driveData, const string& dirname)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);
	workhorse.getDir(dirname); // TODO can this fail?
}

} // namespace openmsx
