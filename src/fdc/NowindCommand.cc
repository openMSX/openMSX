#include "NowindCommand.hh"
#include "NowindRomDisk.hh"
#include "NowindInterface.hh"
#include "DiskChanger.hh"
#include "DSKDiskImage.hh"
#include "DiskPartition.hh"
#include "FileContext.hh"
#include "StringOp.hh"
#include "FileOperations.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "array_ref.hh"
#include "memory.hh"
#include "unreachable.hh"
#include <cassert>

using std::unique_ptr;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

NowindCommand::NowindCommand(const string& basename,
                             CommandController& commandController_,
                             NowindInterface& interface_)
	: Command(commandController_, basename)
	, interface(interface_)
{
}

unique_ptr<DiskChanger> NowindCommand::createDiskChanger(
	const string& basename, unsigned n, MSXMotherBoard& motherBoard) const
{
	string driveName = StringOp::Builder() << basename << n + 1;
	return make_unique<DiskChanger>(motherBoard, driveName, false, true);
}

unsigned NowindCommand::searchRomdisk(const NowindHost::Drives& drives) const
{
	for (unsigned i = 0; i < drives.size(); ++i) {
		if (drives[i]->isRomdisk()) {
			return i;
		}
	}
	return 255;
}

void NowindCommand::processHdimage(
	string_ref hdimage, NowindHost::Drives& drives) const
{
	MSXMotherBoard& motherboard = interface.getMotherBoard();

	// Possible formats are:
	//   <filename> or <filename>:<range>
	// Though <filename> itself can contain ':' characters. To solve this
	// disambiguity we will always interpret the string as <filename> if
	// it is an existing filename.
	set<unsigned> partitions;
	auto pos = hdimage.find_last_of(':');
	if ((pos != string::npos) && !FileOperations::exists(hdimage)) {
		partitions = StringOp::parseRange(
			hdimage.substr(pos + 1), 1, 31);
	}

	auto wholeDisk = std::make_shared<DSKDiskImage>(Filename(hdimage.str()));
	bool failOnError = true;
	if (partitions.empty()) {
		// insert all partitions
		failOnError = false;
		for (unsigned i = 1; i <= 31; ++i) {
			partitions.insert(i);
		}
	}

	for (auto& p : partitions) {
		try {
			// Explicit conversion to shared_ptr<SectorAccessibleDisk> is
			// for some reason needed in 32-bit vs2013 build (not in 64-bit
			// and not in vs2012, nor gcc/clang). Compiler bug???
			auto partition = make_unique<DiskPartition>(
				*wholeDisk, p,
				std::shared_ptr<SectorAccessibleDisk>(wholeDisk));
			auto drive = createDiskChanger(
				interface.basename, unsigned(drives.size()),
				motherboard);
			drive->changeDisk(unique_ptr<Disk>(std::move(partition)));
			drives.push_back(std::move(drive));
		} catch (MSXException&) {
			if (failOnError) throw;
		}
	}
}

void NowindCommand::execute(array_ref<TclObject> tokens, TclObject& result)
{
	auto& host = interface.host;
	auto& drives = interface.drives;
	unsigned oldRomdisk = searchRomdisk(drives);

	if (tokens.size() == 1) {
		// no arguments, show general status
		assert(!drives.empty());
		StringOp::Builder r;
		for (unsigned i = 0; i < drives.size(); ++i) {
			r << "nowind" << i + 1 << ": ";
			if (dynamic_cast<NowindRomDisk*>(drives[i].get())) {
				r << "romdisk\n";
			} else if (auto changer = dynamic_cast<DiskChanger*>(
						drives[i].get())) {
				string filename = changer->getDiskName().getOriginal();
				r << (filename.empty() ? "--empty--" : filename)
				  << '\n';
			} else {
				UNREACHABLE;
			}
		}
		r << "phantom drives: "
		  << (host.getEnablePhantomDrives() ? "enabled" : "disabled")
		  << '\n';
		r << "allow other diskroms: "
		  << (host.getAllowOtherDiskroms() ? "yes" : "no")
		  << '\n';
		result.setString(r);
		return;
	}

	// first parse complete commandline and store state in these local vars
	bool enablePhantom = false;
	bool disablePhantom = false;
	bool allowOther = false;
	bool disallowOther = false;
	bool changeDrives = false;
	unsigned romdisk = 255;
	NowindHost::Drives tmpDrives;
	string error;

	// actually parse the commandline
	array_ref<TclObject> args(&tokens[1], tokens.size() - 1);
	while (error.empty() && !args.empty()) {
		bool createDrive = false;
		string_ref image;

		string_ref arg = args.front().getString();
		args.pop_front();
		if        ((arg == "--ctrl")    || (arg == "-c")) {
			enablePhantom  = false;
			disablePhantom = true;
		} else if ((arg == "--no-ctrl") || (arg == "-C")) {
			enablePhantom  = true;
			disablePhantom = false;
		} else if ((arg == "--allow")    || (arg == "-a")) {
			allowOther    = true;
			disallowOther = false;
		} else if ((arg == "--no-allow") || (arg == "-A")) {
			allowOther    = false;
			disallowOther = true;

		} else if ((arg == "--romdisk") || (arg == "-j")) {
			if (romdisk != 255) {
				error = "Can only have one romdisk";
			} else {
				romdisk = unsigned(tmpDrives.size());
				tmpDrives.push_back(make_unique<NowindRomDisk>());
				changeDrives = true;
			}

		} else if ((arg == "--image") || (arg == "-i")) {
			if (args.empty()) {
				error = "Missing argument for option: " + arg;
			} else {
				image = args.front().getString();
				args.pop_front();
				createDrive = true;
			}

		} else if ((arg == "--hdimage") || (arg == "-m")) {
			if (args.empty()) {
				error = "Missing argument for option: " + arg;
			} else {
				try {
					string_ref hdimage = args.front().getString();
					args.pop_front();
					processHdimage(hdimage, tmpDrives);
					changeDrives = true;
				} catch (MSXException& e) {
					error = e.getMessage();
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
			if (!image.empty()) {
				if (drive->insertDisk(image)) {
					error = "Invalid disk image: " + image;
				}
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
		if (allowOther && !host.getAllowOtherDiskroms()) {
			host.setAllowOtherDiskroms(true);
			optionsChanged = true;
		}
		if (disallowOther && host.getAllowOtherDiskroms()) {
			host.setAllowOtherDiskroms(false);
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
	for (auto& d : drives) {
		if (auto disk = dynamic_cast<DiskChanger*>(d.get())) {
			disk->createCommand();
		}
	}

	if (!error.empty()) {
		throw CommandException(error);
	}

	// calculate result string
	string r;
	if (changeDrives && (prevSize != drives.size())) {
		r += "Number of drives changed. ";
	}
	if (changeDrives && (romdisk != oldRomdisk)) {
		if (oldRomdisk == 255) {
			r += "Romdisk added. ";
		} else if (romdisk == 255) {
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
	result.setString(r);
}

string NowindCommand::help(const vector<string>& /*tokens*/) const
{
	return "Similar to the disk<x> commands there is a nowind<x> command "
	       "for each nowind interface. This command is modeled after the "
	       "'usbhost' command of the real nowind interface. Though only a "
	       "subset of the options is supported. Here's a short overview.\n"
	       "\n"
	       "Command line options\n"
	       " long      short explanation\n"
	       "--image    -i    specify disk image\n"
	       "--hdimage  -m    specify harddisk image\n"
	       "--romdisk  -j    enable romdisk\n"
	     // "--flash    -f    update firmware\n"
	       "--ctrl     -c    no phantom disks\n"
	       "--no-ctrl  -C    enable phantom disks\n"
	       "--allow    -a    allow other diskroms to initialize\n"
	       "--no-allow -A    don't allow other diskroms to initialize\n"
	     //"--dsk2rom  -z    converts a 360kB disk to romdisk.bin\n"
	     //"--debug    -d    enable libnowind debug info\n"
	     //"--test     -t    testmode\n"
	     //"--help     -h    help message\n"
	       "\n"
	       "If you don't pass any arguments to this command, you'll get "
	       "an overview of the current nowind status.\n"
	       "\n"
	       "This command will create a certain amount of drives on the "
	       "nowind interface and (optionally) insert diskimages in those "
	       "drives. For each of these drives there will also be a "
	       "'nowind<1..8>' command created. Those commands are similar to "
	       "e.g. the diska command. They can be used to access the more "
	       "advanced diskimage insertion options. See 'help nowind<1..8>' "
	       "for details.\n"
	       "\n"
	       "In some cases it is needed to reboot the MSX before the "
	       "changes take effect. In those cases you'll get a message "
	       "that warns about this.\n"
	       "\n"
	       "Examples:\n"
	       "nowinda -a image.dsk -j     Image.dsk is inserted into drive A: and the romdisk\n"
	       "                            will be drive B:. Other diskroms will be able to\n"
	       "                            install drives as well. For example when the MSX has\n"
	       "                            an internal diskdrive, drive C: en D: will be\n"
	       "                            available as well.\n"
	       "nowinda disk1.dsk disk2.dsk The two images will be inserted in A: and B:\n"
	       "                            respectively.\n"
	       "nowinda -m hdimage.dsk      Inserts a harddisk image. All available partitions\n"
	       "                            will be mounted as drives.\n"
               "nowinda -m hdimage.dsk:1    Inserts the first partition only.\n"
	       "nowinda -m hdimage.dsk:2-4  Inserts the 2nd, 3th and 4th partition as drive A:\n"
	       "                            B: and C:.\n";
}

void NowindCommand::tabCompletion(vector<string>& tokens) const
{
	static const char* const extra[] = {
		"-c", "--ctrl",
		"-C", "--no-ctrl",
		"-a", "--allow",
		"-A", "--no-allow",
		"-j", "--romdisk",
		"-i", "--image",
		"-m", "--hdimage",
	};
	completeFileName(tokens, userFileContext(), extra);
}

} // namespace openmsx
