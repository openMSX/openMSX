#include "DiskManipulator.hh"
#include "DiskContainer.hh"
#include "MSXtar.hh"
#include "DiskImageUtils.hh"
#include "DSKDiskImage.hh"
#include "DiskPartition.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Reactor.hh"
#include "File.hh"
#include "Filename.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "SectorBasedDisk.hh"
#include "StringOp.hh"
#include "TclObject.hh"
#include "memory.hh"
#include "xrange.hh"
#include <cassert>
#include <ctype.h>
#include <stdexcept>

using std::string;
using std::vector;
using std::unique_ptr;

namespace openmsx {

#ifndef _MSC_EXTENSIONS
// #ifdef required to avoid link error with vc++, see also
//   http://www.codeguru.com/forum/showthread.php?t=430949
const unsigned DiskManipulator::MAX_PARTITIONS;
#endif

DiskManipulator::DiskManipulator(CommandController& commandController,
                                 Reactor& reactor_)
	: Command(commandController, "diskmanipulator")
	, reactor(reactor_)
{
}

DiskManipulator::~DiskManipulator()
{
	assert(drives.empty()); // all DiskContainers must be unregistered
}

string DiskManipulator::getMachinePrefix() const
{
	string id = reactor.getMachineID();
	return id.empty() ? id : id + "::";
}

void DiskManipulator::registerDrive(
	DiskContainer& drive, const std::string& prefix)
{
	assert(findDriveSettings(drive) == end(drives));
	DriveSettings driveSettings;
	driveSettings.drive = &drive;
	driveSettings.driveName = prefix + drive.getContainerName();
	driveSettings.partition = 0;
	for (unsigned i = 0; i <= MAX_PARTITIONS; ++i) {
		driveSettings.workingDir[i] = '/';
	}
	drives.push_back(driveSettings);
}

void DiskManipulator::unregisterDrive(DiskContainer& drive)
{
	auto it = findDriveSettings(drive);
	assert(it != end(drives));
	drives.erase(it);
}

DiskManipulator::Drives::iterator DiskManipulator::findDriveSettings(
	DiskContainer& drive)
{
	return find_if(begin(drives), end(drives),
	               [&](DriveSettings& ds) { return ds.drive == &drive; });
}

DiskManipulator::Drives::iterator DiskManipulator::findDriveSettings(
	string_ref name)
{
	return find_if(begin(drives), end(drives),
	               [&](DriveSettings& ds) { return ds.driveName == name; });
}

DiskManipulator::DriveSettings& DiskManipulator::getDriveSettings(
	string_ref diskname)
{
	// first split-off the end numbers (if present)
	// these will be used as partition indication
	auto pos1 = diskname.find("::");
	auto tmp1 = (pos1 == string_ref::npos) ? diskname : diskname.substr(pos1);
	auto pos2 = tmp1.find_first_of("0123456789");
	auto pos1b = (pos1 == string_ref::npos) ? 0 : pos1;
	auto tmp2 = diskname.substr(0, pos2 + pos1b);

	auto it = findDriveSettings(tmp2);
	if (it == end(drives)) {
		it = findDriveSettings(getMachinePrefix() + tmp2);
		if (it == end(drives)) {
			throw CommandException("Unknown drive: " + tmp2);
		}
	}

	auto* disk = it->drive->getSectorAccessibleDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type.");
	}

	if (pos2 == string_ref::npos) {
		// whole disk
		it->partition = 0;
	} else {
		try {
			unsigned partition = fast_stou(diskname.substr(pos2));
			DiskImageUtils::checkFAT12Partition(*disk, partition);
			it->partition = partition;
		} catch (std::invalid_argument&) {
			// parse error in fast_stou()
			throw CommandException("Invalid partition name");
		}
	}
	return *it;
}

unique_ptr<DiskPartition> DiskManipulator::getPartition(
	const DriveSettings& driveData)
{
	auto* disk = driveData.drive->getSectorAccessibleDisk();
	assert(disk);
	return make_unique<DiskPartition>(*disk, driveData.partition);
}


void DiskManipulator::execute(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");
	}

	string_ref subcmd = tokens[1].getString();
	if (((tokens.size() != 4)                     && (subcmd == "savedsk")) ||
	    ((tokens.size() != 4)                     && (subcmd == "mkdir"))   ||
	    ((tokens.size() != 3)                     && (subcmd == "dir"))     ||
	    ((tokens.size() < 3 || tokens.size() > 4) && (subcmd == "format"))  ||
	    ((tokens.size() < 3 || tokens.size() > 4) && (subcmd == "chdir"))   ||
	    ((tokens.size() < 4)                      && (subcmd == "export"))  ||
	    ((tokens.size() < 4)                      && (subcmd == "import"))  ||
	    ((tokens.size() < 4)                      && (subcmd == "create"))) {
		throw CommandException("Incorrect number of parameters");
	}

	if (subcmd == "export") {
		string_ref dir = tokens[3].getString();
		if (!FileOperations::isDirectory(dir)) {
			throw CommandException(dir + " is not a directory");
		}
		auto& settings = getDriveSettings(tokens[2].getString());
		array_ref<TclObject> lists(std::begin(tokens) + 4, std::end(tokens));
		exprt(settings, dir, lists);

	} else if (subcmd == "import") {
		auto& settings = getDriveSettings(tokens[2].getString());
		array_ref<TclObject> lists(std::begin(tokens) + 3, std::end(tokens));
		result.setString(import(settings, lists));

	} else if (subcmd == "savedsk") {
		auto& settings = getDriveSettings(tokens[2].getString());
		savedsk(settings, tokens[3].getString());

	} else if (subcmd == "chdir") {
		auto& settings = getDriveSettings(tokens[2].getString());
		if (tokens.size() == 3) {
			result.setString("Current directory: " +
			                 settings.workingDir[settings.partition]);
		} else {
			result.setString(chdir(settings, tokens[3].getString()));
		}

	} else if (subcmd == "mkdir") {
		auto& settings = getDriveSettings(tokens[2].getString());
		mkdir(settings, tokens[3].getString());

	} else if (subcmd == "create") {
		create(tokens);

	} else if (subcmd == "format") {
		bool dos1 = false;
		string_ref drive = tokens[2].getString();
		if (tokens.size() == 4) {
			if (drive == "-dos1") {
				dos1 = true;
				drive = tokens[3].getString();
			} else if (tokens[3].getString() == "-dos1") {
				dos1 = true;
			}
		}
		auto& settings = getDriveSettings(drive);
		format(settings, dos1);

	} else if (subcmd == "dir") {
		auto& settings = getDriveSettings(tokens[2].getString());
		result.setString(dir(settings));

	} else {
		throw CommandException("Unknown subcommand: " + subcmd);
	}
}

string DiskManipulator::help(const vector<string>& tokens) const
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

void DiskManipulator::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		static const char* const cmds[] = {
			"import", "export", "savedsk", "dir", "create",
			"format", "chdir", "mkdir",
		};
		completeString(tokens, cmds);

	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		completeFileName(tokens, UserFileContext());

	} else if (tokens.size() == 3) {
		vector<string> names;
		if ((tokens[1] == "format") || (tokens[1] == "create")) {
			names.push_back("-dos1");
		}
		for (auto& d : drives) {
			const auto& name1 = d.driveName; // with prexix
			const auto& name2 = d.drive->getContainerName(); // without prefix
			names.insert(end(names), {name1, name2});
			// if it has partitions then we also add the partition
			// numbers to the autocompletion
			if (auto* disk = d.drive->getSectorAccessibleDisk()) {
				for (unsigned i = 1; i <= MAX_PARTITIONS; ++i) {
					try {
						DiskImageUtils::checkFAT12Partition(*disk, i);
						names.insert(end(names), {
							name1 + StringOp::toString(i),
							name2 + StringOp::toString(i)});
					} catch (MSXException&) {
						// skip invalid partition
					}
				}
			}
		}
		completeString(tokens, names);

	} else if (tokens.size() >= 4) {
		if ((tokens[1] == "savedsk") ||
		    (tokens[1] == "import")  ||
		    (tokens[1] == "export")) {
			completeFileName(tokens, UserFileContext());
		} else if (tokens[1] == "create") {
			static const char* const cmds[] = {
				"360", "720", "32M", "-dos1"
			};
			completeString(tokens, cmds);
		} else if (tokens[1] == "format") {
			static const char* const cmds[] = {
				"-dos1"
			};
			completeString(tokens, cmds);
		}
	}
}

void DiskManipulator::savedsk(const DriveSettings& driveData,
                              string_ref filename)
{
	auto partition = getPartition(driveData);
	SectorBuffer buf;
	File file(filename, File::CREATE);
	for (auto i : xrange(partition->getNbSectors())) {
		partition->readSector(i, buf);
		file.write(&buf, sizeof(buf));
	}
}

void DiskManipulator::create(array_ref<TclObject> tokens)
{
	vector<unsigned> sizes;
	unsigned totalSectors = 0;
	bool dos1 = false;

	for (unsigned i = 3; i < tokens.size(); ++i) {
		if (tokens[i].getString() == "-dos1") {
			dos1 = true;
			continue;
		}

		if (sizes.size() >= MAX_PARTITIONS) {
			throw CommandException(StringOp::Builder() <<
				"Maximum number of partitions is " << MAX_PARTITIONS);
		}
		string tok = tokens[i].getString().str();
		char* q;
		int sectors = strtol(tok.c_str(), &q, 0);
		int scale = 1024; // default is kilobytes
		if (*q) {
			if ((q == tok.c_str()) || *(q + 1)) {
				throw CommandException("Invalid size: " + tok);
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
	if (sizes.empty()) {
		throw CommandException("No size(s) given.");
	}
	if (sizes.size() > 1) {
		// extra sector for partition table
		++totalSectors;
	}

	// create file with correct size
	Filename filename(tokens[2].getString().str());
	try {
		File file(filename, File::CREATE);
		file.truncate(totalSectors * SectorBasedDisk::SECTOR_SIZE);
	} catch (FileException& e) {
		throw CommandException("Couldn't create image: " + e.getMessage());
	}

	// initialize (create partition tables and format partitions)
	DSKDiskImage image(filename);
	if (sizes.size() > 1) {
		DiskImageUtils::partition(image, sizes);
	} else {
		// only one partition specified, don't create partition table
		DiskImageUtils::format(image, dos1);
	}
}

void DiskManipulator::format(DriveSettings& driveData, bool dos1)
{
	auto partition = getPartition(driveData);
	DiskImageUtils::format(*partition, dos1);
	driveData.workingDir[driveData.partition] = '/';
}

unique_ptr<MSXtar> DiskManipulator::getMSXtar(
	SectorAccessibleDisk& disk, DriveSettings& driveData)
{
	if (DiskImageUtils::hasPartitionTable(disk)) {
		throw CommandException(
			"Please select partition number.");
	}

	auto result = make_unique<MSXtar>(disk);
	try {
		result->chdir(driveData.workingDir[driveData.partition]);
	} catch (MSXException&) {
		driveData.workingDir[driveData.partition] = '/';
		throw CommandException(
		    "Directory " + driveData.workingDir[driveData.partition] +
		    " doesn't exist anymore. Went back to root "
		    "directory. Command aborted, please retry.");
	}
	return result;
}

string DiskManipulator::dir(DriveSettings& driveData)
{
	auto partition = getPartition(driveData);
	unique_ptr<MSXtar> workhorse = getMSXtar(*partition, driveData);
	return workhorse->dir();
}

string DiskManipulator::chdir(DriveSettings& driveData, string_ref filename)
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(*partition, driveData);
	try {
		workhorse->chdir(filename);
	} catch (MSXException& e) {
		throw CommandException("chdir failed: " + e.getMessage());
	}
	// TODO clean-up this temp hack, used to enable relative paths
	string& cwd = driveData.workingDir[driveData.partition];
	if (StringOp::startsWith(filename, '/')) {
		cwd = filename.str();
	} else {
		if (!StringOp::endsWith(cwd, '/')) cwd += '/';
		cwd.append(filename.data(), filename.size());
	}
	return "New working directory: " + cwd;
}

void DiskManipulator::mkdir(DriveSettings& driveData, string_ref filename)
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(*partition, driveData);
	try {
		workhorse->mkdir(filename);
	} catch (MSXException& e) {
		throw CommandException(e.getMessage());
	}
}

string DiskManipulator::import(DriveSettings& driveData,
                               array_ref<TclObject> lists)
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(*partition, driveData);

	string messages;
	auto& interp = getInterpreter();
	for (auto& l : lists) {
		for (auto i : xrange(l.getListLength(interp))) {
			auto s = l.getListIndex(interp, i).getString();
			try {
				FileOperations::Stat st;
				if (!FileOperations::getStat(s, st)) {
					throw CommandException(
						"Non-existing file " + s);
				}
				if (FileOperations::isDirectory(st)) {
					messages += workhorse->addDir(s);
				} else if (FileOperations::isRegularFile(st)) {
					messages += workhorse->addFile(s.str());
				} else {
					// ignore other stuff (sockets, device nodes, ..)
					messages += "Ignoring " + s + '\n';
				}
			} catch (MSXException& e) {
				throw CommandException(e.getMessage());
			}
		}
	}
	return messages;
}

void DiskManipulator::exprt(DriveSettings& driveData, string_ref dirname,
                            array_ref<TclObject> lists)
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(*partition, driveData);
	try {
		if (lists.empty()) {
			// export all
			workhorse->getDir(dirname);
		} else {
			for (auto& l : lists) {
				workhorse->getItemFromDir(dirname, l.getString());
			}
		}
	} catch (MSXException& e) {
		throw CommandException(e.getMessage());
	}
}

} // namespace openmsx
