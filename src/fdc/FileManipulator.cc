// $Id$

#include "FileManipulator.hh"
#include "DiskContainer.hh"
#include "DiskChanger.hh"
#include "MSXtar.hh"
#include "DSKDiskImage.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "File.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "SectorBasedDisk.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "Interpreter.hh"
#include <cassert>
#include <ctype.h>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

FileManipulator::FileManipulator()
{
	CommandController::instance().registerCommand(this, "diskmanipulator");

	virtualDrive.reset(new DiskChanger("virtual_drive", *this));
}

FileManipulator::~FileManipulator()
{
	virtualDrive.reset();

	assert(diskImages.empty()); // all DiskContainers must be unregistered
	CommandController::instance().unregisterCommand(this, "diskmanipulator");
}

FileManipulator& FileManipulator::instance()
{
	static FileManipulator oneInstance;
	return oneInstance;
}

void FileManipulator::registerDrive(DiskContainer& drive, const string& imageName)
{
	assert(diskImages.find(imageName) == diskImages.end());
	diskImages[imageName].drive = &drive;
	diskImages[imageName].partition = 0;
	for (int i = 0; i < 31; ++i) {
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
	string tmp = diskname.substr(0, pos);
	DiskImages::iterator it = diskImages.find(tmp);
	if (it == diskImages.end()) {
		throw CommandException("Unknown drive: "  + tmp);
	}

	it->second.partition = 0;
	if (pos != string::npos) {
		int i = strtol(diskname.substr(pos).c_str(), NULL, 10);
		if (i <= 0 || i > 31) {
			throw CommandException("Invalid partition specified.");
		}
		it->second.partition = i - 1;
	}
	return it->second;
}

SectorAccessibleDisk& FileManipulator::getDisk(const DriveSettings& driveData)
{
	SectorAccessibleDisk* disk = driveData.drive->getSectorAccessibleDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type.");
	}
	return *disk;
}


string FileManipulator::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");

	} else if ((tokens.size() != 4 && ( tokens[1] == "savedsk"
	                                || tokens[1] == "mkdir"
	                                || tokens[1] == "export"))
	        || (tokens.size() != 3 && (tokens[1] == "dir"
	                                || tokens[1] == "format"))
	        || ((tokens.size() < 3 || tokens.size() > 4) &&
	                                  (tokens[1] == "chdir"))
	        || (tokens.size() < 4 && tokens[1] == "import")
	        || (tokens.size() <= 3 && (tokens[1] == "create"))) {
		throw CommandException("Incorrect number of parameters");

	} else if ( tokens[1] == "export" ) {
		if (!FileOperations::isDirectory(tokens[3])) {
			throw CommandException(tokens[3] + " is not a directory");
		}
		DriveSettings& settings = getDriveSettings(tokens[2]);
		exprt (settings, tokens[3]);

	} else if (tokens[1] == "import" ) {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		vector<string> lists(tokens.begin() + 3, tokens.end());
		import(settings, lists);

	} else if (tokens[1] == "savedsk") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		savedsk(settings, tokens[3]);

	} else if (tokens[1] == "chdir") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		if (tokens.size() == 3){
			result += "Current directory: " +
			          settings.workingDir[settings.partition];
		} else {
			chdir(settings, tokens[3], result);
		}

	} else if (tokens[1] == "mkdir") {
		DriveSettings& settings = getDriveSettings(tokens[2]);
		mkdir(settings, tokens[3]);

	} else if (tokens[1] == "create") {
		create(tokens);

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
	    "diskmanipulator import <disk name> <host directory|host file>\n"
	    "Import all files and subdirs from the host OS as specified into the\n"
	    "<disk name> in the current MSX subdirectory as was specified with the\n"
	    "last chdir command.\n";
	  } else if (tokens[1] == "export" ) {
	  helptext=
	    "diskmanipulator export <disk name> <host directory>\n"
	    "Extract all files and subdirs from the MSX subdirectory specified with\n"
	    "the chdir command from <disk name> to the host OS in <host directory>.\n";
	  } else if (tokens[1] == "savedsk") {
	  helptext=
	    "diskmanipulator savedsk <disk name> <dskfilename>\n"
	    "This saves the complete drive content to <dskfilename>, it is not possible to\n"
	    "save just one partition. The main purpose of this command is to make it\n"
	    "possible to save a 'ramdsk' into a file and to take 'live backups' of\n"
	    "dsk-files in use.\n";
	  } else if (tokens[1] == "chdir") {
	  helptext=
	    "diskmanipulator chdir <disk name> <MSX directory>\n"
	    "Change the working directory on <disk name>. This will be the\n"
	    "directory were the 'import', 'export' and 'dir' commands will\n"
	    "work on.\n"
	    "In case of a partitioned drive, each partition has its own\n"
	    "working directory.\n";
	  } else if (tokens[1] == "mkdir") {
	  helptext=
	    "diskmanipulator mkdir <disk name> <MSX directory>\n"
	    "This creates the directory on <disk name>. If needed, all missing\n"
	    "parent directories are created at the same time. Accepts both\n"
	    "absolute and relative pathnames.\n";
	  } else if (tokens[1] == "create") {
	  helptext=
	    "diskmanipulator create <dskfilename> <size/option> [<size/option>...]\n"
	    "Creates a formatted dsk file with the given size.\n"
	    "If multiple sizes are given, a partitioned disk image will\n"
	    "be created with each partition having the size as indicated. By\n"
	    "default the sizes are expressed in kilobyte, add the postfix M\n"
	    "for megabyte.\n";
	  } else if (tokens[1] == "format") {
	  helptext=
	    "diskmanipulator format <disk name>\n"
	    "formats the current (partition on) <disk name> with a regular\n"
	    "FAT12 MSX filesystem with an MSX-DOS2 boot sector.\n";
	  } else if (tokens[1] == "dir") {
	  helptext=
	    "diskmanipulator dir <disk name>\n"
	    "Shows the content of the current directory on <disk name>\n";
	  } else {
	  helptext = "Unknown diskmanipulator subcommand: " + tokens[1];
	  }
	} else {
	  helptext=
	    "diskmanipulator create <fn> <sz> [<sz> ...]  : create a formatted dsk file with name <fn>\n"
	    "                                               having the given (partition) size(s)\n"
	    "diskmanipulator savedsk <disk name> <fn>     : save <disk name> as dsk file named as <fn>\n"
	    "diskmanipulator format <disk name>           : format (a partition) on <disk name>\n"
	    "diskmanipulator chdir <disk name> <MSX dir>  : change directory on <disk name>\n"
	    "diskmanipulator mkdir <disk name> <MSX dir>  : create directory on <disk name>\n"
	    "diskmanipulator dir <disk name>              : long format file listing of current\n"
	    "                                               directory on <disk name>\n"
	    "diskmanipulator import <disk> <dir/file> ... : import files and subdirs from <dir/file>\n"
	    "diskmanipulator export <disk> <host dir>     : export all files on <disk> to <host dir>\n"
	    "For more info use 'help diskmanipulator <subcommand>'.\n";
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
		cmds.insert("dir");
		cmds.insert("create");
		cmds.insert("format");
		cmds.insert("chdir");
		cmds.insert("mkdir");
		CommandController::completeString(tokens, cmds);

	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		CommandController::completeFileName(tokens);

	} else if (tokens.size() == 3) {
		set<string> names;
		for (DiskImages::const_iterator it = diskImages.begin();
		     it != diskImages.end(); ++it) {
			names.insert(it->first);
			// if it has partitions then we also add the partition
			// numbers to the autocompletion
			SectorAccessibleDisk* sectorDisk =
				it->second.drive->getSectorAccessibleDisk();
			if (sectorDisk != NULL) {
				try {
					MSXtar workhorse(*sectorDisk);
					for (int i = 0; i < 31; ++i) {
						if (workhorse.hasPartition(i)) {
							names.insert(it->first + StringOp::toString(i + 1));
						}
					}
				} catch (MSXException& e) {
					// ignore
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

void FileManipulator::create(const vector<string>& tokens)
{
	vector<unsigned> sizes;
	unsigned totalSectors = 0;
	for (unsigned i = 3; i < tokens.size(); ++i) {
		char* q;
		int sectors = strtol(tokens[i].c_str(), &q, 0);
		int scale = 1024; // default is kilobytes
		if (*q) {
			if ((q == tokens[i].c_str()) || *(q + 1)) {
				throw CommandException(
					"Invalid size: " + tokens[i]);
			}
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
				default:
					throw CommandException(
					    string("Invalid postfix: ") + q);
			}
		}
		sectors = (sectors * scale) / SectorBasedDisk::SECTOR_SIZE;
		// for a 32MB disk or greater the sectors would be >= 65536
		// since MSX use 16 bits for this, in case of sectors = 65536
		// the truncated word will be 0 -> formatted as 320 Kb disk!
		if (sectors > 65535) sectors = 65535; // this is the max size for fat12 :-)

		// TEMP FIX: the smallest bootsector we create in MSXtar is for
		// a normal single sided disk.
		// TODO: MSXtar must be altered and this temp fix must be set to
		// the real smallest dsk possible (= bootsecor + minimal fat +
		// minial dir + minimal data clusters)
		if (sectors < 720) sectors = 720;

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

	// initialize (create partition tables and format partitions)
	DSKDiskImage image(tokens[2]);
	MSXtar workhorse(image);
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
	if (!workhorse.hasPartitionTable()) {
		workhorse.usePartition(0); //read the bootsector
	} else if (!workhorse.usePartition(driveData.partition)) {
		throw CommandException(
		    "Partition " + StringOp::toString(1 + driveData.partition) +
		    " doesn't exist on this device. Command aborted, please retry.");
	}
	if (!workhorse.chdir(driveData.workingDir[driveData.partition])) {
		driveData.workingDir[driveData.partition] = "\\";
		throw CommandException(
		    "Directory " + driveData.workingDir[driveData.partition] +
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

void FileManipulator::import(DriveSettings& driveData,
                             const vector<string>& lists)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);

	for (vector<string>::const_iterator it = lists.begin();
	     it != lists.end(); ++it) {
		vector<string> list;
		Interpreter::instance().splitList(*it, list);

		for (vector<string>::const_iterator it = list.begin();
		     it != list.end(); ++it) {
			if (FileOperations::isDirectory(*it)) {
				workhorse.addDir(*it); // TODO can this fail?
			} else if (FileOperations::isRegularFile(*it)) {
				workhorse.addFile(*it);
			}
			// TODO: do we warn the user when trying to import sockets/device-nodes/links?
		}
	}
}

void FileManipulator::exprt(DriveSettings& driveData, const string& dirname)
{
	MSXtar workhorse(getDisk(driveData));
	restoreCWD(workhorse, driveData);
	workhorse.getDir(dirname); // TODO can this fail?
}

} // namespace openmsx
