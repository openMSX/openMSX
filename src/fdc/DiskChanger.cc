// $Id$

#include "DiskChanger.hh"
#include "DummyDisk.hh"
#include "RamDSKDiskImage.hh"
#include "XSADiskImage.hh"
#include "FDC_DirAsDSK.hh"
#include "DSKDiskImage.hh"
#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "CommandController.hh"
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

DiskChanger::DiskChanger(const string& driveName_,
                         FileManipulator& manipulator_)
	: driveName(driveName_), manipulator(manipulator_)
{
	XMLElement& config = GlobalSettings::instance().getMediaConfig();
	XMLElement& diskConfig = config.getCreateChild(driveName);
	diskElem = &diskConfig.getCreateChild("filename");
	string filename = diskElem->getData();
	if (!filename.empty()) {
		try {
			FileContext& context = diskConfig.getFileContext();
			vector<string> patchFiles;
			XMLElement::Children children;
			diskConfig.getChildren("ips", children);
			for (XMLElement::Children::const_iterator it = children.begin();
			     it != children.end(); ++it) {
				string patch = context.resolve((*it)->getData());
				patchFiles.push_back(patch);
			}

			insertDisk(context.resolve(filename), patchFiles);
		} catch (FileException& e) {
			// file not found
			throw FatalError("Couldn't load diskimage: " + filename);
		}
	} else {
		// nothing specified
		ejectDisk();
	}

	if (CommandController::instance().hasCommand(driveName)) {
		throw FatalError("Duplicated drive name: " + driveName);
	}

	// only register when everything went ok (no exceptions)
	manipulator.registerDrive(*this, driveName);
	CommandController::instance().registerCommand(this, driveName);
}

DiskChanger::~DiskChanger()
{
	CommandController::instance().unregisterCommand(this, driveName);
	manipulator.unregisterDrive(*this, driveName);
}

const string& DiskChanger::getDriveName() const
{
	return driveName;
}

const string& DiskChanger::getDiskName() const
{
	return diskElem->getData();
}

bool DiskChanger::diskChanged()
{
	bool ret = diskChangedFlag;
	diskChangedFlag = false;
	return ret;
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
				disk.reset(new FDC_DirAsDSK(diskImage));
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
	diskElem->setData(diskImage);
	diskChangedFlag = true;
	CliComm::instance().update(CliComm::MEDIA, getDriveName(), getDiskName());
}

void DiskChanger::ejectDisk()
{
	disk.reset(new DummyDisk());

	diskElem->setData("");
	diskChangedFlag = true;
	CliComm::instance().update(CliComm::MEDIA, getDriveName(), getDiskName());
}


void DiskChanger::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() == 1) {
		const string& diskName = getDiskName();
		result.addListElement(getDriveName() + ':');
		result.addListElement(diskName);

		TclObject options(result.getInterpreter());
		if (dynamic_cast<DummyDisk*>(disk.get())) {
			options.addListElement("empty");
		} else if (dynamic_cast<FDC_DirAsDSK*>(disk.get())) {
			options.addListElement("dirasdisk");
		} else if (dynamic_cast<RamDSKDiskImage*>(disk.get())) {
			options.addListElement("ramdsk");
		}
		if (disk->writeProtected()) {
			options.addListElement("readonly");
		}
		if (options.getListLength() != 0) {
			result.addListElement(options);
		}

	} else if (tokens[1]->getString() == "-ramdsk") {
		vector<string> nopatchfiles;
		insertDisk(tokens[1]->getString(), nopatchfiles);
	} else if (tokens[1]->getString() == "-eject") {
		ejectDisk();
	} else if (tokens[1]->getString() == "eject") {
		ejectDisk();
		result.setString(
			"Warning: use of 'eject' is deprecated, instead use '-eject'");
	} else {
		try {
			UserFileContext context;
			vector<string> patches;
			for (unsigned i = 2; i < tokens.size(); ++i) {
				patches.push_back(context.resolve(
					tokens[i]->getString()));
			}
			insertDisk(context.resolve(tokens[1]->getString()), patches);
		} catch (FileException &e) {
			throw CommandException(e.getMessage());
		}
	}
}

string DiskChanger::help(const vector<string>& /*tokens*/) const
{
	const string& name = getDriveName();
	return name + " -eject      : remove disk from virtual drive\n" +
	       name + " -ramdsk     : create a virtual disk in RAM\n" +
	       name + " <filename> : change the disk file\n";
}

void DiskChanger::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		set<string> extra;
		extra.insert("-eject");
		extra.insert("-ramdsk");
		UserFileContext context;
		CommandController::completeFileName(tokens, context, extra);
	}
}

} // namespace openmsx
