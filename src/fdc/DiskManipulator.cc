#include "DiskManipulator.hh"

#include "CommandException.hh"
#include "DSKDiskImage.hh"
#include "DiskContainer.hh"
#include "DiskImageUtils.hh"
#include "DiskPartition.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "Filename.hh"
#include "Keyboard.hh"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXtar.hh"
#include "Reactor.hh"
#include "SectorBasedDisk.hh"

#include "StringOp.hh"
#include "TclArgParser.hh"
#include "TclObject.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <array>
#include <cassert>
#include <cctype>
#include <memory>
#include <ranges>

using std::string;
using std::string_view;

namespace openmsx {

DiskManipulator::DiskManipulator(CommandController& commandController_,
                                 Reactor& reactor_)
	: Command(commandController_, "diskmanipulator")
	, reactor(reactor_)
{
}

DiskManipulator::~DiskManipulator()
{
	assert(drives.empty()); // all DiskContainers must be unregistered
}

string DiskManipulator::getMachinePrefix() const
{
	string_view id = reactor.getMachineID();
	return id.empty() ? string{} : strCat(id, "::");
}

void DiskManipulator::registerDrive(
	DiskContainer& drive, std::string_view prefix)
{
	assert(findDriveSettings(drive) == end(drives));
	DriveSettings driveSettings;
	driveSettings.drive = &drive;
	driveSettings.driveName = strCat(prefix, drive.getContainerName());
	driveSettings.partition = 0;
	drives.push_back(driveSettings);
}

void DiskManipulator::unregisterDrive(DiskContainer& drive)
{
	auto it = findDriveSettings(drive);
	assert(it != end(drives));
	move_pop_back(drives, it);
}

std::vector<std::string> DiskManipulator::getDriveNamesForCurrentMachine() const
{
	std::vector<std::string> result;
	result.emplace_back("virtual_drive"); // always available

	auto prefix = getMachinePrefix();
	if (prefix.empty()) return result;

	for (const auto& drive : drives) {
		if (!drive.driveName.starts_with(prefix)) continue;

		const auto* disk = drive.drive->getSectorAccessibleDisk();
		if (disk && DiskImageUtils::hasPartitionTable(*disk)) {
			for (unsigned i = 1; true; ++i) {
				try {
					SectorBuffer buf;
					DiskImageUtils::getPartition(*disk, i, buf);
				} catch (MSXException&) {
					break; // stop when the partition doesn't exist
				}
				try {
					DiskImageUtils::checkSupportedPartition(*disk, i);
					result.push_back(strCat(
						drive.driveName.substr(prefix.size()), i));
				} catch (MSXException&) {
					// skip invalid partition
				}
			}
		} else {
			// empty drive or no partition table
			result.emplace_back(drive.driveName.substr(prefix.size()));
		}
	}
	return result;
}

DiskContainer* DiskManipulator::getDrive(std::string_view fullName) const
{
	// input does not have machine prefix, but it may have a partition suffix
	auto pos = fullName.find_first_of("0123456789");
	auto driveName = (pos != std::string_view::npos)
		? fullName.substr(0, pos) // drop partition number
		: fullName;

	auto it = ranges::find(drives, driveName, &DriveSettings::driveName);
	if (it == end(drives)) {
		it = ranges::find(drives, tmpStrCat(getMachinePrefix(), driveName), &DriveSettings::driveName);
		if (it == end(drives)) {
			return {}; // drive doesn't exist
		}
	}
	auto* drive = it->drive;
	assert(drive);
	return drive;
}

std::optional<DiskManipulator::DriveAndPartition> DiskManipulator::getDriveAndDisk(std::string_view fullName) const
{
	auto* drive = getDrive(fullName);
	if (!drive) return {};
	auto* disk = drive->getSectorAccessibleDisk();
	if (!disk) return {};

	// input does not have machine prefix, but it may have a partition suffix
	auto pos = fullName.find_first_of("0123456789");
	unsigned partitionNum = 0; // full disk
	if (pos != std::string_view::npos) {
		auto num = StringOp::stringToBase<10, unsigned>(fullName.substr(pos));
		if (!num) return {}; // parse error
		partitionNum = *num;
	}
	try {
		return DriveAndPartition{
			drive,
			std::make_unique<DiskPartition>(*disk, partitionNum)};
	} catch (MSXException&) {
		return {}; // invalid partition?
	}
}

std::string DiskManipulator::DriveSettings::getWorkingDir(unsigned p)
{
	return p < workingDir.size() ? workingDir[p] : "/";
}

void DiskManipulator::DriveSettings::setWorkingDir(unsigned p, std::string_view dir)
{
	if (p >= workingDir.size()) {
		workingDir.resize(p + 1, "/");
	}
	workingDir[p] = dir;
}

DiskManipulator::Drives::iterator DiskManipulator::findDriveSettings(
	DiskContainer& drive)
{
	return ranges::find(drives, &drive, &DriveSettings::drive);
}

DiskManipulator::Drives::iterator DiskManipulator::findDriveSettings(
	string_view driveName)
{
	return ranges::find(drives, driveName, &DriveSettings::driveName);
}

DiskManipulator::DriveSettings& DiskManipulator::getDriveSettings(
	string_view diskName)
{
	// first split-off the end numbers (if present)
	// these will be used as partition indication
	auto pos1 = diskName.find("::");
	auto tmp1 = (pos1 == string_view::npos) ? diskName : diskName.substr(pos1);
	auto pos2 = tmp1.find_first_of("0123456789");
	auto pos1b = (pos1 == string_view::npos) ? 0 : pos1;
	auto tmp2 = diskName.substr(0, pos2 + pos1b);

	auto it = findDriveSettings(tmp2);
	if (it == end(drives)) {
		it = findDriveSettings(tmpStrCat(getMachinePrefix(), tmp2));
		if (it == end(drives)) {
			throw CommandException("Unknown drive: ", tmp2);
		}
	}

	const auto* disk = it->drive->getSectorAccessibleDisk();
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type.");
	}

	if (pos2 == string_view::npos) {
		// whole disk
		it->partition = 0;
	} else {
		auto partitionName = diskName.substr(pos2);
		auto partition = StringOp::stringToBase<10, unsigned>(partitionName);
		if (!partition) {
			throw CommandException("Invalid partition name: ", partitionName);
		}
		DiskImageUtils::checkSupportedPartition(*disk, *partition);
		it->partition = *partition;
	}
	return *it;
}

DiskPartition DiskManipulator::getPartition(
	const DriveSettings& driveData)
{
	auto* disk = driveData.drive->getSectorAccessibleDisk();
	assert(disk);
	return {*disk, driveData.partition};
}

static std::optional<MSXBootSectorType> parseBootSectorType(std::string_view s) {
	if (s == "-dos1")   return MSXBootSectorType::DOS1;
	if (s == "-dos2")   return MSXBootSectorType::DOS2;
	if (s == "-nextor") return MSXBootSectorType::NEXTOR;
	return {};
};

static size_t parseSectorSize(zstring_view tok) {
	char* q;
	size_t bytes = strtoull(tok.c_str(), &q, 0);
	int scale = 1024; // default is kilobytes
	if (*q) {
		if ((q == tok.c_str()) || *(q + 1)) {
			throw CommandException("Invalid size: ", tok);
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
				throw CommandException("Invalid suffix: ", q);
		}
	}
	size_t sectors = (bytes * scale) / SectorBasedDisk::SECTOR_SIZE;

	// TEMP FIX: the smallest boot sector we create in MSXtar is for
	// a normal single sided disk.
	// TODO: MSXtar must be altered and this temp fix must be set to
	// the real smallest dsk possible (= boot sector + minimal fat +
	// minimal dir + minimal data clusters)
	return std::max(sectors, size_t(720));
}

void DiskManipulator::execute(std::span<const TclObject> tokens, TclObject& result)
{
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");
	}

	string_view subCmd = tokens[1].getString();
	if (((tokens.size() != 4)                     && (subCmd == one_of("savedsk", "mkdir", "delete"))) ||
	    ((tokens.size() != 5)                     && (subCmd ==        "rename"          ))  ||
	    ((tokens.size() != 3)                     && (subCmd ==        "dir"             ))  ||
	    ((tokens.size() < 3 || tokens.size() > 4) && (subCmd ==        "chdir"           ))  ||
	    ((tokens.size() < 3 || tokens.size() > 5) && (subCmd ==        "format"          ))  ||
	    ((tokens.size() < 3)                      && (subCmd ==        "partition"       ))  ||
	    ((tokens.size() < 4)                      && (subCmd == one_of("export", "import", "create")))) {
		throw CommandException("Incorrect number of parameters");
	}

	if (subCmd == "export") {
		string_view dir = tokens[3].getString();
		auto directory = FileOperations::expandTilde(string(dir));
		if (!FileOperations::isDirectory(directory)) {
			throw CommandException(dir, " is not a directory");
		}
		auto& settings = getDriveSettings(tokens[2].getString());
		std::span<const TclObject> lists(std::begin(tokens) + 4, std::end(tokens));
		exprt(settings, directory, lists);

	} else if (subCmd == "import") {
		auto& settings = getDriveSettings(tokens[2].getString());

		bool overwrite = false;
		std::array info = {flagArg("-overwrite", overwrite)};
		auto lists = parseTclArgs(getInterpreter(), tokens.subspan(3), info);
		if (lists.empty()) throw SyntaxError();

		result = imprt(settings, lists, overwrite);

	} else if (subCmd == "savedsk") {
		const auto& settings = getDriveSettings(tokens[2].getString());
		savedsk(settings, FileOperations::expandTilde(string(tokens[3].getString())));

	} else if (subCmd == "chdir") {
		auto& settings = getDriveSettings(tokens[2].getString());
		if (tokens.size() == 3) {
			result = tmpStrCat("Current directory: ",
			                   settings.getWorkingDir(settings.partition));
		} else {
			result = chdir(settings, tokens[3].getString());
		}

	} else if (subCmd == "mkdir") {
		auto& settings = getDriveSettings(tokens[2].getString());
		mkdir(settings, tokens[3].getString());

	} else if (subCmd == "create") {
		create(tokens);

	} else if (subCmd == "partition") {
		partition(tokens);

	} else if (subCmd == "format") {
		format(tokens);

	} else if (subCmd == "dir") {
		auto& settings = getDriveSettings(tokens[2].getString());
		result = dir(settings);

	} else if (subCmd == "delete") {
		auto& settings = getDriveSettings(tokens[2].getString());
		result = deleteEntry(settings, tokens[3].getString());

	} else if (subCmd == "rename") {
		auto& settings = getDriveSettings(tokens[2].getString());
		result = rename(settings, tokens[3].getString(), tokens[4].getString());

	} else {
		throw CommandException("Unknown subcommand: ", subCmd);
	}
}

string DiskManipulator::help(std::span<const TclObject> tokens) const
{
	string helpText;
	if (tokens.size() >= 2) {
	  if (tokens[1] == "import") {
	  helpText =
	    "diskmanipulator import <disk name> [-overwrite] <host directory|host file>\n"
	    "Import all files and subdirs from the host OS as specified into the <disk name> in the\n"
	    "current MSX subdirectory as was specified with the last chdir command.\n"
	    "By default already existing entries are not overwritten, unless the -overwrite option is used.";
	  } else if (tokens[1] == "export") {
	  helpText =
	    "diskmanipulator export <disk name> <host directory>\n"
	    "Extract all files and subdirs from the MSX subdirectory specified with the chdir command\n"
	    "from <disk name> to the host OS in <host directory>.\n";
	  } else if (tokens[1] == "savedsk") {
	  helpText =
	    "diskmanipulator savedsk <disk name> <dskfilename>\n"
	    "Save the complete drive content to <dskfilename>, it is not possible to save just one\n"
	    "partition. The main purpose of this command is to make it possible to save a 'ramdsk' into\n"
	    "a file and to take 'live backups' of dsk-files in use.\n";
	  } else if (tokens[1] == "chdir") {
	  helpText =
	    "diskmanipulator chdir <disk name> <MSX directory>\n"
	    "Change the working directory on <disk name>. This will be the directory were the 'import',\n"
	    "'export' and 'dir' commands will work on.\n"
	    "In case of a partitioned drive, each partition has its own working directory.\n";
	  } else if (tokens[1] == "mkdir") {
	  helpText =
	    "diskmanipulator mkdir <disk name> <MSX directory>\n"
	    "Create the specified directory on <disk name>. If needed, all missing parent directories\n"
	    "are created at the same time. Accepts both absolute and relative path names.\n";
	  } else if (tokens[1] == "create") {
	  helpText =
	    "diskmanipulator create <dskfilename> <size/option> [<size/option>...]\n"
	    "Create a formatted dsk file with the given size.\n"
	    "If multiple sizes are given, a partitioned disk image will be created with each partition\n"
	    "having the size as indicated. By default the sizes are expressed in kilobyte, add the\n"
	    "postfix M for megabyte.\n"
	    "When using the -dos1 option, the boot sector of the created image will be MSX-DOS1\n"
	    "compatible. When using the -nextor option, the boot sector and partition table will be\n"
		"Nextor compatible, and FAT16 volumes can be created.\n";
	  } else if (tokens[1] == "partition") {
	  helpText =
	    "diskmanipulator partition <disk name> [<size/option>...]\n"
	    "Partitions and formats the current <disk name> to the indicated sizes. By default the\n"
	    "sizes are expressed in kilobyte, add the postfix M for megabyte.\n"
	    "When using the -dos1 option, the boot sector of the disk will be MSX-DOS1 compatible.\n"
	    "When using the -nextor option, the boot sector and partition table will be Nextor\n"
		"compatible, and FAT16 volumes can be created.\n";
	  } else if (tokens[1] == "format") {
	  helpText =
	    "diskmanipulator format <disk name>\n"
	    "Formats the current (partition on) <disk name>. By default, it will create a regular\n"
	    "FAT12 MSX file system with an MSX-DOS2 boot sector, or, when the -dos1 option is\n"
		"specified, with an MSX-DOS1 boot sector. When the -nextor option is specified, it\n"
		"will create a FAT12 or FAT16 file system, with a Nextor boot sector.\n";
	  } else if (tokens[1] == "dir") {
	  helpText =
	    "diskmanipulator dir <disk name>\n"
	    "Shows the content of the current directory on <disk name>\n";
	  } else if (tokens[1] == "delete") {
	  helpText =
	    "diskmanipulator delete <disk name> <MSX entry>\n"
	    "Delete the given entry (a file or a directory) from disk, freeing up disk space.\n"
	    "The entry is searched in the directory specified with the chdir command.\n"
	    "In case of a directory (recursively) also all entries in that directory are deleted.\n";
	  } else if (tokens[1] == "rename") {
	  helpText =
	    "diskmanipulator rename <disk name> <old> <new>\n"
	    "Search the entry (file or directory) with name <old> and give it the name <new>.\n"
	    "The entry is searched in the directory specified with the chdir command.\n"
	    "The new name may not already exist.\n";
	  } else {
	  helpText = strCat("Unknown diskmanipulator subcommand: ", tokens[1].getString());
	  }
	} else {
	  helpText =
	    "diskmanipulator create <fn> <sz> [<sz> ...]  : create a formatted dsk file with name <fn>\n"
	    "                                               having the given (partition) size(s)\n"
	    "diskmanipulator partition <dn> [<sz> ...]    : partition and format <disk name>\n"
	    "diskmanipulator savedsk <disk name> <fn>     : save <disk name> as dsk file named as <fn>\n"
	    "diskmanipulator format <disk name> [<sz>]    : format (a partition on) <disk name>\n"
	    "diskmanipulator chdir <disk name> <MSX dir>  : change directory on <disk name>\n"
	    "diskmanipulator mkdir <disk name> <MSX dir>  : create directory on <disk name>\n"
	    "diskmanipulator dir <disk name>              : long format file listing of current\n"
	    "                                               directory on <disk name>\n"
	    "diskmanipulator import <disk> <dir/file> ... : import files and subdirs from <dir/file>\n"
	    "diskmanipulator export <disk> <host dir>     : export all files on <disk> to <host dir>\n"
	    "diskmanipulator delete <disk> <MSX entry>    : delete the specified file or directory\n"
	    "diskmanipulator rename <disk> <old> <new>    : rename file or directory from <old> to <new>\n"
	    "For more info use 'help diskmanipulator <subcommand>'.\n";
	}
	return helpText;
}

void DiskManipulator::tabCompletion(std::vector<string>& tokens) const
{
	using namespace std::literals;
	if (tokens.size() == 2) {
		static constexpr std::array cmds = {
			"import"sv, "export"sv, "savedsk"sv, "dir"sv, "create"sv,
			"partition"sv, "format"sv, "chdir"sv, "mkdir"sv, "delete"sv,
			"rename"sv
		};
		completeString(tokens, cmds);

	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		completeFileName(tokens, userFileContext());

	} else if (tokens.size() == 3) {
		std::vector<string> names;
		if (tokens[1] == one_of("partition", "format")) {
			names.emplace_back("-dos1");
			names.emplace_back("-dos2");
			names.emplace_back("-nextor");
		}
		for (const auto& d : drives) {
			const auto& name1 = d.driveName; // with prefix
			const auto& name2 = d.drive->getContainerName(); // without prefix
			append(names, {name1, std::string(name2)});
			// if it has partitions then we also add the partition
			// numbers to the autocompletion
			if (const auto* disk = d.drive->getSectorAccessibleDisk()) {
				for (unsigned i = 1; true; ++i) {
					try {
						SectorBuffer buf;
						DiskImageUtils::getPartition(*disk, i, buf);
					} catch (MSXException&) {
						break;
					}
					try {
						DiskImageUtils::checkSupportedPartition(*disk, i);
						append(names,
						       {strCat(name1, i), strCat(name2, i)});
					} catch (MSXException&) {
						// skip invalid partition
					}
				}
			}
		}
		completeString(tokens, names);

	} else if (tokens.size() >= 4) {
		if (tokens[1] == one_of("savedsk", "import", "export")) {
			static constexpr std::array extra = {"-overwrite"sv};
			completeFileName(tokens, userFileContext(),
				(tokens[1] == "import") ? extra : std::span<const std::string_view>{});
		} else if (tokens[1] == one_of("create", "partition", "format")) {
			static constexpr std::array cmds = {
				"360"sv, "720"sv, "32M"sv, "-dos1"sv, "-dos2"sv, "-nextor"sv,
			};
			completeString(tokens, cmds);
		}
	}
}

void DiskManipulator::savedsk(const DriveSettings& driveData,
                              string filename) const
{
	auto partition = getPartition(driveData);
	SectorBuffer buf;
	File file(std::move(filename), File::OpenMode::CREATE);
	for (auto i : xrange(partition.getNbSectors())) {
		partition.readSector(i, buf);
		file.write(buf.raw);
	}
}

static std::pair<MSXBootSectorType, std::vector<unsigned>> parsePartitionSizes(auto tokens)
{
	MSXBootSectorType bootType = MSXBootSectorType::DOS2;
	std::vector<unsigned> sizes;

	for (const auto& token_ : tokens) {
		if (auto t = parseBootSectorType(token_.getString())) {
			bootType = *t;
		} else if (size_t sectors = parseSectorSize(token_.getString());
		           sectors <= std::numeric_limits<unsigned>::max()) {
			sizes.push_back(narrow<unsigned>(sectors));
		} else {
			throw CommandException("Partition size too large.");
		}
	}

	return {bootType, sizes};
}

void DiskManipulator::create(std::span<const TclObject> tokens) const
{
	auto [bootType, sizes] = parsePartitionSizes(std::views::drop(tokens, 3));
	auto filename = FileOperations::expandTilde(string(tokens[2].getString()));
	create(filename, bootType, sizes);
}

void DiskManipulator::create(const std::string& filename_, MSXBootSectorType bootType, const std::vector<unsigned>& sizes) const
{
	size_t totalSectors = sum(sizes, [](size_t s) { return s; });
	if (totalSectors == 0) {
		throw CommandException("No size(s) given.");
	}

	// create file with correct size
	Filename filename(filename_);
	try {
		File file(filename, File::OpenMode::CREATE);
		file.truncate(totalSectors * SectorBasedDisk::SECTOR_SIZE);
	} catch (FileException& e) {
		throw CommandException("Couldn't create image: ", e.getMessage());
	}

	// initialize (create partition tables and format partitions)
	DSKDiskImage image(filename);
	if (sizes.size() > 1) {
		unsigned partitionCount = DiskImageUtils::partition(image,
			static_cast<std::span<const unsigned>>(sizes), bootType);
		if (partitionCount != sizes.size()) {
			throw CommandException("Could not create all partitions; ",
				partitionCount, " of ", sizes.size(), " created.");
		}
	} else {
		// only one partition specified, don't create partition table
		DiskImageUtils::format(image, bootType);
	}
}

void DiskManipulator::partition(std::span<const TclObject> tokens)
{
	auto [bootType, sizes] = parsePartitionSizes(std::views::drop(tokens, 3));

	// initialize (create partition tables and format partitions)
	auto& settings = getDriveSettings(tokens[2].getString());
	if (settings.partition > 0) {
		throw CommandException("Disk name must not have partition number.");
	}
	auto* image = settings.drive->getSectorAccessibleDisk();
	unsigned partitionCount = DiskImageUtils::partition(*image,
		static_cast<std::span<const unsigned>>(sizes), bootType);
	if (partitionCount != sizes.size()) {
		throw CommandException("Could not create all partitions; ",
			partitionCount, " of ", sizes.size(), " created.");
	}
}

void DiskManipulator::format(std::span<const TclObject> tokens)
{
	MSXBootSectorType bootType = MSXBootSectorType::DOS2;
	std::optional<string> drive;
	std::optional<size_t> size;
	for (const auto& token_ : std::views::drop(tokens, 2)) {
		if (auto t = parseBootSectorType(token_.getString())) {
			bootType = *t;
		} else if (!drive) {
			drive = token_.getString();
		} else if (!size) {
			size = parseSectorSize(token_.getString());
		} else {
			throw CommandException("Incorrect number of parameters");
		}
	}
	if (drive) {
		auto& driveData = getDriveSettings(*drive);
		auto partition = getPartition(driveData);
		if (size) {
			DiskImageUtils::format(partition, bootType, *size);
		} else {
			DiskImageUtils::format(partition, bootType);
		}
		driveData.setWorkingDir(driveData.partition, "/");
	} else {
		throw CommandException("Incorrect number of parameters");
	}
}

MSXtar DiskManipulator::getMSXtar(
	SectorAccessibleDisk& disk, DriveSettings& driveData) const
{
	if (DiskImageUtils::hasPartitionTable(disk)) {
		throw CommandException("Please select partition number.");
	}

	MSXtar result(disk, reactor.getMsxChar2Unicode());
	string cwd = driveData.getWorkingDir(driveData.partition);
	try {
		result.chdir(cwd);
	} catch (MSXException&) {
		driveData.setWorkingDir(driveData.partition, "/");
		throw CommandException(
			"Directory ", cwd,
			" doesn't exist anymore. Went back to root "
			"directory. Command aborted, please retry.");
	}
	return result;
}

string DiskManipulator::dir(DriveSettings& driveData) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);
	return workhorse.dir();
}

std::string DiskManipulator::deleteEntry(DriveSettings& driveData, std::string_view entry) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);
	return workhorse.deleteItem(entry);
}

std::string DiskManipulator::rename(DriveSettings& driveData, std::string_view oldName, std::string_view newName) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);
	return workhorse.renameItem(oldName, newName);
}

string DiskManipulator::chdir(DriveSettings& driveData, string_view filename) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);
	try {
		workhorse.chdir(filename);
	} catch (MSXException& e) {
		throw CommandException("chdir failed: ", e.getMessage());
	}
	// TODO clean-up this temp hack, used to enable relative paths
	string cwd = driveData.getWorkingDir(driveData.partition);
	if (filename.starts_with('/')) {
		cwd = filename;
	} else {
		if (!cwd.ends_with('/')) cwd += '/';
		cwd.append(filename.data(), filename.size());
	}
	driveData.setWorkingDir(driveData.partition, cwd);
	return "New working directory: " + cwd;
}

void DiskManipulator::mkdir(DriveSettings& driveData, string_view filename) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);
	try {
		workhorse.mkdir(filename);
	} catch (MSXException& e) {
		throw CommandException(std::move(e).getMessage());
	}
}

string DiskManipulator::imprt(DriveSettings& driveData,
                              std::span<const TclObject> lists, bool overwrite) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);

	string messages;
	auto& interp = getInterpreter();
	auto add = overwrite ? MSXtar::Add::OVERWRITE
	                     : MSXtar::Add::PRESERVE;
	for (const auto& l : lists) {
		for (auto i : xrange(l.getListLength(interp))) {
			auto s = FileOperations::expandTilde(string(l.getListIndex(interp, i).getString()));
			try {
				auto st = FileOperations::getStat(s);
				if (!st) {
					throw CommandException("Non-existing file ", s);
				}
				if (FileOperations::isDirectory(*st)) {
					messages += workhorse.addDir(s, add);
				} else if (FileOperations::isRegularFile(*st)) {
					messages += workhorse.addFile(s, add);
				} else {
					// ignore other stuff (sockets, device nodes, ..)
					strAppend(messages, "Ignoring ", s, '\n');
				}
			} catch (MSXException& e) {
				throw CommandException(std::move(e).getMessage());
			}
		}
	}
	return messages;
}

void DiskManipulator::exprt(DriveSettings& driveData, string_view dirname,
                            std::span<const TclObject> lists) const
{
	auto partition = getPartition(driveData);
	auto workhorse = getMSXtar(partition, driveData);
	try {
		if (lists.empty()) {
			// export all
			workhorse.getDir(dirname);
		} else {
			for (const auto& l : lists) {
				workhorse.getItemFromDir(dirname, l.getString());
			}
		}
	} catch (MSXException& e) {
		throw CommandException(std::move(e).getMessage());
	}
}

} // namespace openmsx
