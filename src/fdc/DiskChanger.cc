// $Id$

#include "DiskChanger.hh"
#include "DummyDisk.hh"
#include "RamDSKDiskImage.hh"
#include "XSADiskImage.hh"
#include "DirAsDSK.hh"
#include "DSKDiskImage.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "MSXEventDistributor.hh"
#include "InputEvents.hh"
#include "Scheduler.hh"
#include "DiskManipulator.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandException.hh"
#include "CliComm.hh"
#include "TclObject.hh"
#include "EmuTime.hh"
#include "checked_cast.hh"

using std::set;
using std::string;
using std::vector;

namespace openmsx {

class DiskCommand : public Command
{
public:
	DiskCommand(CommandController& commandController,
	            DiskChanger& diskChanger);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	DiskChanger& diskChanger;
};

DiskChanger::DiskChanger(const string& driveName_,
                         CommandController& commandController,
                         DiskManipulator& manipulator_,
                         MSXEventDistributor* msxEventDistributor_,
                         Scheduler* scheduler_)
	: cliComm(commandController.getCliComm())
	, globalSettings(commandController.getGlobalSettings())
	, msxEventDistributor(msxEventDistributor_)
	, scheduler(scheduler_)
	, manipulator(manipulator_)
	, driveName(driveName_)
	, diskCommand(new DiskCommand(commandController, *this))
{
	ejectDisk();
	manipulator.registerDrive(*this);
	if (msxEventDistributor) {
		msxEventDistributor->registerEventListener(*this);
	}
}

DiskChanger::~DiskChanger()
{
	if (msxEventDistributor) {
		msxEventDistributor->unregisterEventListener(*this);
	}
	manipulator.unregisterDrive(*this);
}

const string& DiskChanger::getDriveName() const
{
	return driveName;
}

const string& DiskChanger::getDiskName() const
{
	return disk->getName();
}

bool DiskChanger::diskChanged()
{
	bool ret = diskChangedFlag;
	diskChangedFlag = false;
	return ret;
}

bool DiskChanger::peekDiskChanged() const
{
	return diskChangedFlag;
}

Disk& DiskChanger::getDisk()
{
	return *disk;
}

SectorAccessibleDisk* DiskChanger::getSectorAccessibleDisk()
{
	return dynamic_cast<SectorAccessibleDisk*>(disk.get());
}

const std::string& DiskChanger::getContainerName() const
{
	return getDriveName();
}

void DiskChanger::sendChangeDiskEvent(const vector<string>& args)
{
	// note: might throw MSXException
	MSXEventDistributor::EventPtr event(new MSXCommandEvent(args));
	if (msxEventDistributor) {
		msxEventDistributor->distributeEvent(
			event, scheduler->getCurrentTime());
	} else {
		signalEvent(event, EmuTime::zero);
	}
}

void DiskChanger::signalEvent(
	shared_ptr<const Event> event, const EmuTime& /*time*/)
{
	if (event->getType() != OPENMSX_MSX_COMMAND_EVENT) return;

	const MSXCommandEvent* commandEvent =
		checked_cast<const MSXCommandEvent*>(event.get());
	const vector<TclObject*>& tokens = commandEvent->getTokens();
	if (tokens[0]->getString() == getDriveName()) {
		if (tokens[1]->getString() == "eject") {
			ejectDisk();
		} else {
			insertDisk(tokens);
		}
	}
}

void DiskChanger::insertDisk(const vector<TclObject*>& args)
{
	std::auto_ptr<Disk> newDisk;
	const string& diskImage = args[1]->getString();
	if (diskImage == "ramdsk") {
		newDisk.reset(new RamDSKDiskImage());
	} else {
		try {
			// first try XSA
			newDisk.reset(new XSADiskImage(diskImage));
		} catch (MSXException& e) {
			try {
				//First try the fake disk, because a DSK will always
				//succeed if diskImage can be resolved
				//It is simply stat'ed, so even a directory name
				//can be resolved and will be accepted as dsk name
				// try to create fake DSK from a dir on host OS
				newDisk.reset(new DirAsDSK(
					cliComm, globalSettings, diskImage));
			} catch (MSXException& e) {
				// then try normal DSK
				newDisk.reset(new DSKDiskImage(diskImage));
			}
		}
	}
	for (unsigned i = 2; i < args.size(); ++i) {
		newDisk->applyPatch(args[i]->getString());
	}

	// no errors, only now replace original disk
	changeDisk(newDisk);
}

void DiskChanger::ejectDisk()
{
	changeDisk(std::auto_ptr<Disk>(new DummyDisk()));
}

void DiskChanger::changeDisk(std::auto_ptr<Disk> newDisk)
{
	disk = newDisk;
	diskChangedFlag = true;
	cliComm.update(CliComm::MEDIA, getDriveName(), getDiskName());
}


// class DiskCommand

DiskCommand::DiskCommand(CommandController& commandController,
                         DiskChanger& diskChanger_)
	: Command(commandController, diskChanger_.driveName)
	, diskChanger(diskChanger_)
{
}

void DiskCommand::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	int firstFileToken = 1;
	if (tokens.size() == 1) {
		result.addListElement(diskChanger.getDriveName() + ':');
		result.addListElement(diskChanger.getDiskName());

		TclObject options(result.getInterpreter());
		if (dynamic_cast<DummyDisk*>(diskChanger.disk.get())) {
			options.addListElement("empty");
		} else if (dynamic_cast<DirAsDSK*>(diskChanger.disk.get())) {
			options.addListElement("dirasdisk");
		} else if (dynamic_cast<RamDSKDiskImage*>(diskChanger.disk.get())) {
			options.addListElement("ramdsk");
		}
		if (diskChanger.disk->writeProtected()) {
			options.addListElement("readonly");
		}
		if (options.getListLength() != 0) {
			result.addListElement(options);
		}

	} else if (tokens[1]->getString() == "ramdsk") {
		vector<string> args;
		args.push_back(diskChanger.getDriveName());
		args.push_back(tokens[1]->getString());
		diskChanger.sendChangeDiskEvent(args);
	} else if (tokens[1]->getString() == "-ramdsk") {
		vector<string> args;
		args.push_back(diskChanger.getDriveName());
		args.push_back("ramdsk");
		diskChanger.sendChangeDiskEvent(args);
		result.setString(
			"Warning: use of '-ramdsk' is deprecated, instead use the 'ramdsk' subcommand");
	} else if (tokens[1]->getString() == "-eject") {
		vector<string> args;
		args.push_back(diskChanger.getDriveName());
		args.push_back("eject");
		diskChanger.sendChangeDiskEvent(args);
		result.setString(
			"Warning: use of '-eject' is deprecated, instead use the 'eject' subcommand");
	} else if (tokens[1]->getString() == "eject") {
		vector<string> args;
		args.push_back(diskChanger.getDriveName());
		args.push_back("eject");
		diskChanger.sendChangeDiskEvent(args);
	} else {
		if (tokens[1]->getString() == "insert") {
			if (tokens.size() > 2) {
				firstFileToken = 2; // skip this subcommand as filearg
			} else {
				throw CommandException("Missing argument to insert subcommand");
			}
		}
		try {
			CommandController& controller = getCommandController();
			UserFileContext context;
			vector<string> args;
			args.push_back(diskChanger.getDriveName());
			for (unsigned i = firstFileToken; i < tokens.size(); ++i) {
				string option = tokens[i]->getString();
				if (option == "-ips") {
					if (++i == tokens.size()) {
						throw MSXException(
							"Missing argument for option \"" + option + "\"");
					}
					args.push_back(context.resolve(
						controller,
						tokens[i]->getString()));
				} else {
					// backwards compatibility
					args.push_back(context.resolve(
						controller, option));
				}
			}
			diskChanger.sendChangeDiskEvent(args);
		} catch (FileException& e) {
			throw CommandException(e.getMessage());
		}
	}
}

string DiskCommand::help(const vector<string>& /*tokens*/) const
{
	const string& name = diskChanger.getDriveName();
	return name + " eject             : remove disk from virtual drive\n" +
	       name + " ramdsk            : create a virtual disk in RAM\n" +
	       name + " insert <filename> : change the disk file\n" +
	       name + " <filename>        : change the disk file\n" +
	       name + "                   : show which disk image is in drive";
}

void DiskCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		set<string> extra;
		extra.insert("eject");
		extra.insert("ramdsk");
		extra.insert("insert");
		UserFileContext context;
		completeFileName(getCommandController(), tokens, context, extra);
	}
}

} // namespace openmsx
