// $Id$

#include "DiskChanger.hh"
#include "DummyDisk.hh"
#include "RamDSKDiskImage.hh"
#include "XSADiskImage.hh"
#include "FDC_DirAsDSK.hh"
#include "DSKDiskImage.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "FileManipulator.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandException.hh"
#include "CliComm.hh"
#include "TclObject.hh"

using std::set;
using std::string;
using std::vector;

namespace openmsx {

class DiskCommand : public Command
{
public:
	DiskCommand(CommandController& commandController,
	            DiskChanger& diskChanger);
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
private:
	DiskChanger& diskChanger;
};

DiskChanger::DiskChanger(const string& driveName_,
                         CommandController& commandController,
                         FileManipulator& manipulator_)
	: driveName(driveName_), manipulator(manipulator_)
	, diskCommand(new DiskCommand(commandController, *this))
	, cliComm(commandController.getCliComm())
{
	ejectDisk();
	manipulator.registerDrive(*this, driveName);
}

DiskChanger::~DiskChanger()
{
	manipulator.unregisterDrive(*this, driveName);
}

const string& DiskChanger::getDriveName() const
{
	return driveName;
}

const string& DiskChanger::getDiskName() const
{
	return diskName;
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

void DiskChanger::insertDisk(const string& diskImage,
                             const vector<string>& patches)
{
	ejectDisk();
	if (diskImage == "-ramdsk") {
		disk.reset(new RamDSKDiskImage());
	} else {
		try {
			// first try XSA
			disk.reset(new XSADiskImage(diskImage));
		} catch (MSXException& e) {
			try {
				//First try the fake disk, because a DSK will always
				//succeed if diskImage can be resolved
				//It is simply stat'ed, so even a directory name
				//can be resolved and will be accepted as dsk name
				// try to create fake DSK from a dir on host OS
				disk.reset(new FDC_DirAsDSK(
					cliComm, diskImage));
			} catch (MSXException& e) {
				// then try normal DSK
				disk.reset(new DSKDiskImage(diskImage));
			}
		}
	}
	for (vector<string>::const_iterator it = patches.begin();
	     it != patches.end(); ++it) {
		disk->applyPatch(*it);
	}
	diskName = diskImage;
	diskChangedFlag = true;
	cliComm.update(CliComm::MEDIA, getDriveName(), getDiskName());
}

void DiskChanger::ejectDisk()
{
	disk.reset(new DummyDisk());

	diskName.clear();
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
	if (tokens.size() == 1) {
		const string& diskName = diskChanger.getDiskName();
		result.addListElement(diskChanger.getDriveName() + ':');
		result.addListElement(diskName);

		TclObject options(result.getInterpreter());
		if (dynamic_cast<DummyDisk*>(diskChanger.disk.get())) {
			options.addListElement("empty");
		} else if (dynamic_cast<FDC_DirAsDSK*>(diskChanger.disk.get())) {
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

	} else if (tokens[1]->getString() == "-ramdsk") {
		vector<string> nopatchfiles;
		diskChanger.insertDisk(tokens[1]->getString(), nopatchfiles);
	} else if (tokens[1]->getString() == "-eject") {
		diskChanger.ejectDisk();
	} else if (tokens[1]->getString() == "eject") {
		diskChanger.ejectDisk();
		result.setString(
			"Warning: use of 'eject' is deprecated, instead use '-eject'");
	} else {
		try {
			UserFileContext context(getCommandController());
			vector<string> patches;
			for (unsigned i = 2; i < tokens.size(); ++i) {
				patches.push_back(context.resolve(
					tokens[i]->getString()));
			}
			diskChanger.insertDisk(
				context.resolve(tokens[1]->getString()), patches);
		} catch (FileException& e) {
			throw CommandException(e.getMessage());
		}
	}
}

string DiskCommand::help(const vector<string>& /*tokens*/) const
{
	const string& name = diskChanger.getDriveName();
	return name + " -eject      : remove disk from virtual drive\n" +
	       name + " -ramdsk     : create a virtual disk in RAM\n" +
	       name + " <filename> : change the disk file\n";
}

void DiskCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		set<string> extra;
		extra.insert("-eject");
		extra.insert("-ramdsk");
		UserFileContext context(getCommandController());
		completeFileName(tokens, context, extra);
	}
}

} // namespace openmsx
