// $Id$

// TODO implement hard disk partition stuff (also in DiskChanger)

#include "NowindInterface.hh"
#include "NowindRomDisk.hh"
#include "NowindHost.hh"
#include "Command.hh"
#include "Rom.hh"
#include "AmdFlash.hh"
#include "DiskChanger.hh"
#include "Clock.hh"
#include "MSXMotherBoard.hh"
#include "FileContext.hh"
#include "StringOp.hh"
#include "Filename.hh"
#include "CommandException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "ref.hh"
#include <bitset>
#include <cassert>
#include <deque>

using std::deque;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

static const unsigned MAX_NOWINDS = 8; // a-h
typedef std::bitset<MAX_NOWINDS> NowindsInUse;


class NowindCommand : public SimpleCommand
{
public:
	NowindCommand(const string& basename,
	              CommandController& commandController,
	              NowindInterface& interface);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	unsigned searchRomdisk(const NowindInterface::Drives& drives) const;
	NowindInterface& interface;
};


static DiskChanger* createDiskChanger(const string& basename, unsigned n,
                                      MSXMotherBoard& motherBoard)
{
	string name = basename + StringOp::toString(n + 1);
	DiskChanger* drive = new DiskChanger(
		name, motherBoard.getCommandController(),
		motherBoard.getDiskManipulator(), &motherBoard, false);
	return drive;
}

NowindInterface::NowindInterface(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, flash(new AmdFlash(*rom, 16, rom->getSize() / (1024 * 64), 0, config))
	, host(new NowindHost(drives))
{
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("nowindsInUse");
	if (info.counter == 0) {
		assert(info.stuff == NULL);
		info.stuff = new NowindsInUse();
	}
	++info.counter;
	NowindsInUse& nowindsInUse = *reinterpret_cast<NowindsInUse*>(info.stuff);

	unsigned i = 0;
	while (nowindsInUse[i]) {
		if (++i == MAX_NOWINDS) {
			throw MSXException("Too many nowind interfaces.");
		}
	}
	nowindsInUse[i] = true;
	basename = string("nowind") + char('a' + i);

	command.reset(new NowindCommand(
		basename, motherBoard.getCommandController(), *this));

	// start with one (empty) drive
	DiskChanger* drive = createDiskChanger(basename, 0, motherBoard);
	drive->createCommand();
	drives.push_back(drive);

	reset(EmuTime::dummy());
}

NowindInterface::~NowindInterface()
{
	deleteDrives();

	MSXMotherBoard::SharedStuff& info =
		getMotherBoard().getSharedStuff("nowindsInUse");
	assert(info.counter);
	assert(info.stuff);
	NowindsInUse& nowindsInUse = *reinterpret_cast<NowindsInUse*>(info.stuff);

	unsigned i = basename[6] - 'a';
	assert(nowindsInUse[i]);
	nowindsInUse[i] = false;

	--info.counter;
	if (info.counter == 0) {
		assert(nowindsInUse.none());
		delete &nowindsInUse;
		info.stuff = NULL;
	}
}

void NowindInterface::deleteDrives()
{
	for (Drives::const_iterator it = drives.begin();
	     it != drives.end(); ++it) {
		delete *it;
	}
}

void NowindInterface::reset(EmuTime::param /*time*/)
{
	// version 1 didn't change the bank number
	// version 2 (produced by Sunrise) does reset the bank number
	bank = 0;

	// Flash state is NOT changed on reset
	//flash->reset();
}

byte NowindInterface::peek(word address, EmuTime::param /*time*/) const
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		return host->peek();
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash->peek(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return 0xFF;
	}
}

byte NowindInterface::readMem(word address, EmuTime::param /*time*/)
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		return host->read();
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash->read(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return 0xFF;
	}
}

const byte* NowindInterface::getReadCacheLine(word address) const
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		// nowind region, not cachable
		return NULL;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash->getReadCacheLine(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return unmappedRead;
	}
}

void NowindInterface::writeMem(word address, byte value, EmuTime::param time)
{
	if (address < 0x4000) {
		flash->write(bank * 0x4000 + address, value);
	} else if (((0x4000 <= address) && (address < 0x6000)) ||
	           ((0x8000 <= address) && (address < 0xA000))) {
		static const Clock<1000> clock(EmuTime::zero);
		host->write(value, clock.getTicksTill(time));
	} else if (((0x6000 <= address) && (address < 0x8000)) ||
	           ((0xA000 <= address) && (address < 0xC000))) {
		byte max = rom->getSize() / (16 * 1024);
		bank = (value < max) ? value : value & (max - 1);
		invalidateMemCache(0x4000, 0x4000);
		invalidateMemCache(0xA000, 0x2000);
	}
}

byte* NowindInterface::getWriteCacheLine(word address) const
{
	if (address < 0xC000) {
		// not cachable
		return NULL;
	} else {
		return unmappedWrite;
	}
}


// class NowindCommand

NowindCommand::NowindCommand(const string& basename,
                             CommandController& commandController,
                             NowindInterface& interface_)
	: SimpleCommand(commandController, basename)
	, interface(interface_)
{
}

unsigned NowindCommand::searchRomdisk(const NowindInterface::Drives& drives) const
{
	for (unsigned i = 0; i < drives.size(); ++i) {
		if (drives[i]->isRomdisk()) {
			return i;
		}
	}
	return 255;
}

string NowindCommand::execute(const vector<string>& tokens)
{
	NowindHost& host = *interface.host;
	NowindInterface::Drives& drives = interface.drives;
	unsigned oldRomdisk = searchRomdisk(drives);

	if (tokens.size() == 1) {
		// no arguments, show general status
		assert(!drives.empty());
		string result;
		for (unsigned i = 0; i < drives.size(); ++i) {
			result += "nowind" + StringOp::toString(i + 1) + ": ";
			if (dynamic_cast<NowindRomDisk*>(drives[i])) {
				result += "romdisk\n";
			} else if (DiskChanger* changer = dynamic_cast<DiskChanger*>(drives[i])) {
				string filename = changer->getDiskName().getOriginal();
				result += filename.empty() ? "--empty--" : filename;
				result += '\n';
			} else {
				assert(false);
			}
		}
		result += string("phantom drives: ") +
		          (host.getEnablePhantomDrives() ? "enabled" : "disabled") +
		          '\n';
		result += string("allow other diskroms: ") +
		          (host.getAllowOtherDiskroms() ? "yes" : "no") +
		          '\n';
		return result;
	}

	// first parse complete commandline and store state in these local vars
	bool enablePhantom = false;
	bool disablePhantom = false;
	bool allowOther = false;
	bool disallowOther = false;
	bool changeDrives = false;
	unsigned romdisk = 255;
	NowindInterface::Drives tmpDrives;
	string error;

	// actually parse the commandline
	deque<string> args(tokens.begin() + 1, tokens.end());
	while (error.empty() && !args.empty()) {
		bool createDrive = false;
		string image;

		string arg = args.front();
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
				romdisk = tmpDrives.size();
				tmpDrives.push_back(new NowindRomDisk());
				changeDrives = true;
			}

		} else if ((arg == "--image") || (arg == "-i")) {
			if (args.empty()) {
				error = "Missing argument for option: " + arg;
			} else {
				image = args.front();
				args.pop_front();
				createDrive = true;
			}

		} else {
			// everything else is interpreted as an image name
			image = arg;
			createDrive = true;
		}

		if (createDrive) {
			DiskChanger* drive = createDiskChanger(
				interface.basename, tmpDrives.size(),
				interface.getMotherBoard());
			tmpDrives.push_back(drive);
			changeDrives = true;
			if (!image.empty()) {
				if (drive->insertDisk(image)) {
					error = "Invalid disk image: " + image;
				}
			}
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
	for (NowindInterface::Drives::const_iterator it = tmpDrives.begin();
	     it != tmpDrives.end(); ++it) {
		delete *it;
	}
	for (NowindInterface::Drives::const_iterator it = drives.begin();
	     it != drives.end(); ++it) {
		if (DiskChanger* disk = dynamic_cast<DiskChanger*>(*it)) {
			disk->createCommand();
		}
	}

	if (!error.empty()) {
		throw CommandException(error);
	}

	// calculate result string
	string result;
	if (changeDrives && (tmpDrives.size() != drives.size())) {
		result += "Number of drives changed. ";
	}
	if (changeDrives && (romdisk != oldRomdisk)) {
		if (oldRomdisk == 255) {
			result += "Romdisk added. ";
		} else if (romdisk == 255) {
			result += "Romdisk removed. ";
		} else {
			result += "Romdisk changed position. ";
		}
	}
	if (optionsChanged) {
		result += "Boot options changed. ";
	}
	if (!result.empty()) {
		result += "You may need to reset the MSX for the changes to take effect.";
	}
	return result;
}

string NowindCommand::help(const vector<string>& tokens) const
{
	return "This command is modeled after the 'usbhost' command of the "
	       "real nowind interface. Though only a subset of the options "
	       "is supported. Here's a short overview.\n"
	       "\n"
	       "Command line options\n"
	       " long      short explanation\n"
	       "--image    -i    specify disk image\n"
	     //"--hdimage  -m    specify harddisk image\n"
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
	       "nowind -a image.dsk -j      Image.dsk is inserted into drive A: and the romdisk\n"
	       "                            will be drive B:. Other diskroms will be able to\n"
	       "                            install drives as well. For example when the MSX has\n"
	       "                            an internal diskdrive, drive C: en D: will be\n"
	       "                            available as well.\n"
	       "nowind disk1.dsk disk2.dsk  The two images will be inserted in A: and B:\n"
	       "                            respectively.\n";
}

void NowindCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> extra;
	extra.insert("--ctrl");     extra.insert("-c");
	extra.insert("--no-ctrl");  extra.insert("-C");
	extra.insert("--allow");    extra.insert("-a");
	extra.insert("--no-allow"); extra.insert("-A");
	extra.insert("--romdisk");  extra.insert("-j");
	extra.insert("--image");    extra.insert("-i");
	UserFileContext context;
	completeFileName(getCommandController(), tokens, context, extra);
}


template<typename Archive>
void NowindInterface::serialize(Archive& ar, unsigned /*version*/)
{
	if (ar.isLoader()) {
		deleteDrives();
	}

	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("flash", *flash);
	ar.serialize("drives", drives, ref(getMotherBoard()));
	ar.serialize("nowindhost", *host);
	ar.serialize("bank", bank);

	// don't serialize command, rom, basename
}
INSTANTIATE_SERIALIZE_METHODS(NowindInterface);
REGISTER_MSXDEVICE(NowindInterface, "NowindInterface");

} // namespace openmsx
