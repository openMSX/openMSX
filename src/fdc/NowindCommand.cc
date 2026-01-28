#include "NowindCommand.hh"

#include "DSKDiskImage.hh"
#include "DiskChanger.hh"
#include "DiskPartition.hh"
#include "NowindInterface.hh"
#include "NowindRomDisk.hh"

#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"

#include "StringOp.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "unreachable.hh"

#include <array>
#include <cassert>
#include <memory>
#include <span>

namespace openmsx {

using namespace std::literals;

NowindCommand::NowindCommand(const std::string& basename,
                             CommandController& commandController_,
                             NowindInterface& interface_)
	: Command(commandController_, basename)
	, interface(interface_)
{
}

std::unique_ptr<DiskChanger> NowindCommand::createDiskChanger(
	const std::string& basename, unsigned n, MSXMotherBoard& motherBoard) const
{
	return std::make_unique<DiskChanger>(
			motherBoard,
			strCat(basename, n + 1),
			false, true);
}

[[nodiscard]] static unsigned searchRomDisk(const NowindHost::Drives& drives)
{
	for (auto [i, drv] : enumerate(drives)) {
		if (drv->isRomDisk()) {
			return unsigned(i);
		}
	}
	return 255;
}

void NowindCommand::processHdimage(
	const std::string& hdImage, NowindHost::Drives& drives) const
{
	MSXMotherBoard& motherboard = interface.getMotherBoard();

	// Possible formats are:
	//   <filename> or <filename>:<range>
	// Though <filename> itself can contain ':' characters. To solve this
	// ambiguity we will always interpret the string as <filename> if
	// it is an existing filename.
	IterableBitSet<64> partitions;
	if (auto pos = hdImage.find_last_of(':');
	    (pos != std::string::npos) && !FileOperations::exists(hdImage)) {
		partitions = StringOp::parseRange(
			std::string_view(hdImage).substr(pos + 1), 1, 31);
	}

	auto wholeDisk = std::make_shared<DSKDiskImage>(Filename(hdImage));
	bool failOnError = true;
	if (partitions.empty()) {
		// insert all partitions
		failOnError = false;
		partitions.setRange(1, 32);
	}

	partitions.foreachSetBit([&](size_t p) {
		try {
			auto partition = std::make_unique<DiskPartition>(
				*wholeDisk, unsigned(p), wholeDisk);
			auto drive = createDiskChanger(
				interface.basename, unsigned(drives.size()),
				motherboard);
			drive->changeDisk(std::unique_ptr<Disk>(std::move(partition)));
			drives.push_back(std::move(drive));
		} catch (MSXException&) {
			if (failOnError) throw;
		}
	});
}

void NowindCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	auto& host = interface.host;
	auto& drives = interface.drives;
	unsigned oldRomDisk = searchRomDisk(drives);

	if (tokens.size() == 1) {
		// no arguments, show general status
		assert(!drives.empty());
		std::string r;
		for (auto [i, drv] : enumerate(drives)) {
			strAppend(r, "nowind", i + 1, ": ");
			if (dynamic_cast<NowindRomDisk*>(drv.get())) {
				strAppend(r, "romdisk\n");
			} else if (const auto* changer = dynamic_cast<const DiskChanger*>(
						drv.get())) {
				std::string filename = changer->getDiskName().getOriginal();
				strAppend(r, (filename.empty() ? "--empty--"sv : std::string_view(filename)),
				          '\n');
			} else {
				UNREACHABLE;
			}
		}
		strAppend(r, "phantom drives: ",
		          (host.getEnablePhantomDrives() ? "enabled"sv : "disabled"sv),
		          "\n"
		          "allow other diskroms: ",
		          (host.getAllowOtherDiskRoms() ? "yes"sv : "no"sv),
		          '\n');
		result = r;
		return;
	}

	// first parse complete command line and store state in these local vars
	bool enablePhantom = false;
	bool disablePhantom = false;
	bool allowOther = false;
	bool disallowOther = false;
	bool changeDrives = false;
	unsigned romDisk = 255;
	NowindHost::Drives tmpDrives;
	std::string error;

	// actually parse the command line
	std::span<const TclObject> args(&tokens[1], tokens.size() - 1);
	while (error.empty() && !args.empty()) {
		bool createDrive = false;
		std::string_view image;

		std::string_view arg = args.front().getString();
		args = args.subspan(1);
		if        (arg == one_of("--ctrl", "-c")) {
			enablePhantom  = false;
			disablePhantom = true;
		} else if (arg == one_of("--no-ctrl", "-C")) {
			enablePhantom  = true;
			disablePhantom = false;
		} else if (arg == one_of("--allow", "-a")) {
			allowOther    = true;
			disallowOther = false;
		} else if (arg == one_of("--no-allow", "-A")) {
			allowOther    = false;
			disallowOther = true;

		} else if (arg == one_of("--romdisk", "-j")) {
			if (romDisk != 255) {
				error = "Can only have one romdisk";
			} else {
				romDisk = unsigned(tmpDrives.size());
				tmpDrives.push_back(std::make_unique<NowindRomDisk>());
				changeDrives = true;
			}

		} else if (arg == one_of("--image", "-i")) {
			if (args.empty()) {
				error = strCat("Missing argument for option: ", arg);
			} else {
				image = args.front().getString();
				args = args.subspan(1);
				createDrive = true;
			}

		} else if (arg == one_of("--hdimage", "-m")) {
			if (args.empty()) {
				error = strCat("Missing argument for option: ", arg);
			} else {
				try {
					auto hdImage = FileOperations::expandTilde(
						std::string(args.front().getString()));
					args = args.subspan(1);
					processHdimage(hdImage, tmpDrives);
					changeDrives = true;
				} catch (MSXException& e) {
					error = std::move(e).getMessage();
				}
			}

		} else {
			// everything else is interpreted as an image name
			image = arg;
			createDrive = true;
		}

		if (createDrive) {
			auto drive = createDiskChanger(
				interface.basename, unsigned(tmpDrives.size()),
				interface.getMotherBoard());
			changeDrives = true;
			if (!image.empty() &&
			    drive->insertDisk(FileOperations::expandTilde(std::string(image)))) {
				error = strCat("Invalid disk image: ", image);
			}
			tmpDrives.push_back(std::move(drive));
		}
	}
	if (tmpDrives.size() > 8) {
		error = "Can't have more than 8 drives";
	}

	// if there was no error, apply the changes
	bool optionsChanged = false;
	if (error.empty()) {
		if (enablePhantom && !host.getEnablePhantomDrives()) {
			host.setEnablePhantomDrives(true);
			optionsChanged = true;
		}
		if (disablePhantom && host.getEnablePhantomDrives()) {
			host.setEnablePhantomDrives(false);
			optionsChanged = true;
		}
		if (allowOther && !host.getAllowOtherDiskRoms()) {
			host.setAllowOtherDiskRoms(true);
			optionsChanged = true;
		}
		if (disallowOther && host.getAllowOtherDiskRoms()) {
			host.setAllowOtherDiskRoms(false);
			optionsChanged = true;
		}
		if (changeDrives) {
			std::swap(tmpDrives, drives);
		}
	}

	// cleanup tmpDrives, this contains either
	//   - the old drives (when command was successful)
	//   - the new drives (when there was an error)
	auto prevSize = tmpDrives.size();
	tmpDrives.clear();
	for (const auto& d : drives) {
		if (auto* disk = dynamic_cast<DiskChanger*>(d.get())) {
			disk->createCommand();
		}
	}

	if (!error.empty()) {
		throw CommandException(error);
	}

	// calculate result string
	std::string r;
	if (changeDrives && (prevSize != drives.size())) {
		r += "Number of drives changed. ";
	}
	if (changeDrives && (romDisk != oldRomDisk)) {
		if (oldRomDisk == 255) {
			r += "Romdisk added. ";
		} else if (romDisk == 255) {
			r += "Romdisk removed. ";
		} else {
			r += "Romdisk changed position. ";
		}
	}
	if (optionsChanged) {
		r += "Boot options changed. ";
	}
	if (!r.empty()) {
		r += "You may need to reset the MSX for the changes to take effect.";
	}
	result = r;
}

std::string NowindCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Similar to the disk<x> commands there is a nowind<x> command "
	       "for each nowind interface. This command is modeled after the "
	       "'usbhost' command of the real nowind interface. Though only a "
	       "subset of the options is supported. Here's a short overview.\n"
	       "\n"
	       "Command line options\n"
	       " long      short explanation\n"
	       "--image    -i    specify disk image\n"
	       "--hdimage  -m    specify hard disk image\n"
	       "--romdisk  -j    enable romdisk\n"
	     // "--flash    -f    update firmware\n"
	       "--ctrl     -c    no phantom disks\n"
	       "--no-ctrl  -C    enable phantom disks\n"
	       "--allow    -a    allow other disk roms to initialize\n"
	       "--no-allow -A    don't allow other disk roms to initialize\n"
	     //"--dsk2rom  -z    converts a 360kB disk to romdisk.bin\n"
	     //"--debug    -d    enable libnowind debug info\n"
	     //"--test     -t    test mode\n"
	     //"--help     -h    help message\n"
	       "\n"
	       "If you don't pass any arguments to this command, you'll get "
	       "an overview of the current nowind status.\n"
	       "\n"
	       "This command will create a certain amount of drives on the "
	       "nowind interface and (optionally) insert disk images in those "
	       "drives. For each of these drives there will also be a "
	       "'nowind<1..8>' command created. Those commands are similar to "
	       "e.g. the diska command. They can be used to access the more "
	       "advanced disk image insertion options. See 'help nowind<1..8>' "
	       "for details.\n"
	       "\n"
	       "In some cases it is needed to reboot the MSX before the "
	       "changes take effect. In those cases you'll get a message "
	       "that warns about this.\n"
	       "\n"
	       "Examples:\n"
	       "nowinda -a image.dsk -j     Image.dsk is inserted into drive A: and the romdisk\n"
	       "                            will be drive B:. Other disk roms will be able to\n"
	       "                            install drives as well. For example when the MSX has\n"
	       "                            an internal disk drive, drive C: en D: will be\n"
	       "                            available as well.\n"
	       "nowinda disk1.dsk disk2.dsk The two images will be inserted in A: and B:\n"
	       "                            respectively.\n"
	       "nowinda -m hdimage.dsk      Inserts a hard disk image. All available partitions\n"
	       "                            will be mounted as drives.\n"
	       "nowinda -m hdimage.dsk:1    Inserts the first partition only.\n"
	       "nowinda -m hdimage.dsk:2-4  Inserts the 2nd, 3th and 4th partition as drive A:\n"
	       "                            B: and C:.\n";
}

void NowindCommand::tabCompletion(std::vector<std::string>& tokens) const
{
	using namespace std::literals;
	static constexpr std::array extra = {
		"-c"sv, "--ctrl"sv,
		"-C"sv, "--no-ctrl"sv,
		"-a"sv, "--allow"sv,
		"-A"sv, "--no-allow"sv,
		"-j"sv, "--romdisk"sv,
		"-i"sv, "--image"sv,
		"-m"sv, "--hdimage"sv,
	};
	completeFileName(tokens, userFileContext(), extra);
}

} // namespace openmsx
