// $Id$

#include "DiskDrive.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "DummyDisk.hh"
#include "XSADiskImage.hh"
#include "DSKDiskImage.hh"
#include "FDC_DirAsDSK.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CliCommOutput.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"
#include "CommandException.hh"
#include "XMLElement.hh"

using std::string;
using std::vector;

namespace openmsx {

/// class DiskDrive ///

DiskDrive::~DiskDrive()
{
}

/// class DummyDrive ///

DummyDrive::DummyDrive()
{
}

DummyDrive::~DummyDrive()
{
}

bool DummyDrive::ready()
{
	return false;
}

bool DummyDrive::writeProtected()
{
	return true;
}

bool DummyDrive::doubleSided()
{
	return false;
}

void DummyDrive::setSide(bool /*side*/)
{
	// ignore
}

void DummyDrive::step(bool /*direction*/, const EmuTime& /*time*/)
{
	// ignore
}

bool DummyDrive::track00(const EmuTime& /*time*/)
{
	return false; // National_FS-5500F1 2nd drive detection depends on this
}

void DummyDrive::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// ignore
}

bool DummyDrive::indexPulse(const EmuTime& /*time*/)
{
	return false;
}

int DummyDrive::indexPulseCount(const EmuTime& /*begin*/,
                                const EmuTime& /*end*/)
{
	return 0;
}

EmuTime DummyDrive::getTimeTillSector(byte sector, const EmuTime& time)
{
	return time;
}

void DummyDrive::setHeadLoaded(bool /*status*/, const EmuTime& /*time*/)
{
	// ignore
}

bool DummyDrive::headLoaded(const EmuTime& /*time*/)
{
	return false;
}

void DummyDrive::read(byte /*sector*/, byte* /*buf*/,
                      byte& /*onDiskTrack*/, byte& /*onDiskSector*/,
                      byte& /*onDiskSide*/,  int&  /*onDiskSize*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::write(byte /*sector*/, const byte* /*buf*/,
                       byte& /*onDiskTrack*/, byte& /*onDiskSector*/,
                       byte& /*onDiskSide*/,  int& /*onDiskSize*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::getSectorHeader(byte /*sector*/, byte* /*buf*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::getTrackHeader(byte* /*buf*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::initWriteTrack()
{
	// ignore ???
}

void DummyDrive::writeTrackData(byte /*data*/)
{
	// ignore ???
}

bool DummyDrive::diskChanged()
{
	return false;
}

bool DummyDrive::dummyDrive()
{
	return true;
}


/// class RealDrive ///

std::bitset<RealDrive::MAX_DRIVES> RealDrive::drivesInUse;

RealDrive::RealDrive(const EmuTime& time)
	: motorStatus(false), motorTimer(time)
	, headLoadStatus(false), headLoadTimer(time)
{
	headPos = 0;
	diskChangedFlag = false;

	int i = 0;
	for ( ; i < MAX_DRIVES; ++i) {
		if (!drivesInUse[i]) {
			name = string("disk") + static_cast<char>('a' + i);
			drivesInUse[i] = true;
			break;
		}
	}
	if (i == MAX_DRIVES) {
		throw FatalError("Too many disk drives.");
	}
	
	XMLElement& config = GlobalSettings::instance().getMediaConfig();
	XMLElement& diskConfig = config.getCreateChild(name);
	diskElem = &diskConfig.getCreateChild("filename");
	const string& filename = diskElem->getData();
	if (!filename.empty()) {
		try {
			FileContext& context = diskConfig.getFileContext();
			string diskImage = context.resolve(filename);

			vector<string> patchFiles;
			XMLElement::Children children;
			diskConfig.getChildren("ips", children);
			for (XMLElement::Children::const_iterator it = children.begin();
			     it != children.end(); ++it) {
				string patch = context.resolve((*it)->getData());
				patchFiles.push_back(patch);
			}
			
			insertDisk(diskImage, patchFiles);
		} catch (FileException &e) {
			// file not found
			throw FatalError("Couldn't load diskimage: " + filename);
		}
	} else {
		// nothing specified
		ejectDisk();
	}
	if (CommandController::instance().hasCommand(name)) {
		throw FatalError("Duplicated drive name: " + name);
	}
	CommandController::instance().registerCommand(this, name);
}

RealDrive::~RealDrive()
{
	int driveNum = name[4] - 'a';
	drivesInUse[driveNum] = false;
	CommandController::instance().unregisterCommand(this, name);
}

bool RealDrive::ready()
{
	return disk->ready();
}

bool RealDrive::writeProtected()
{
	return disk->writeProtected();
}

void RealDrive::step(bool direction, const EmuTime& /*time*/)
{
	if (direction) {
		// step in
		if (headPos < MAX_TRACK) {
			headPos++;
		}
	} else {
		// step out
		if (headPos > 0) {
			headPos--;
		}
	}
	PRT_DEBUG("DiskDrive track " << headPos);
}

bool RealDrive::track00(const EmuTime& /*time*/)
{
	return headPos == 0;
}

void RealDrive::setMotor(bool status, const EmuTime& time)
{
	if (motorStatus != status) {
		motorStatus = status;
		motorTimer.advance(time);
		/* The following is a hack to emulate the drive LED behaviour.
		 * This is in real life dependent on the FDC and should be
		 * investigated in detail to implement it properly... TODO */
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::FDD, motorStatus));
	}
}

bool RealDrive::indexPulse(const EmuTime& time)
{
	if (!motorStatus && disk->ready()) {
		return false;
	}
	int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	return angle < INDEX_DURATION;
}

int RealDrive::indexPulseCount(const EmuTime& begin,
                               const EmuTime& end)
{
	if (!motorStatus && disk->ready()) {
		return 0;
	}
	int t1 = motorTimer.before(begin) ? motorTimer.getTicksTill(begin) : 0;
	int t2 = motorTimer.before(end)   ? motorTimer.getTicksTill(end)   : 0;
	return (t2 / TICKS_PER_ROTATION) - (t1 / TICKS_PER_ROTATION);
}

EmuTime RealDrive::getTimeTillSector(byte sector, const EmuTime& time)
{
	if (!motorStatus && disk->ready()) { // TODO is this correct?
		return time;
	}
	// TODO this really belongs in the Disk class
	int sectorAngle = ((sector - 1) * (TICKS_PER_ROTATION / 9)) %
	                  TICKS_PER_ROTATION;

	int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	int delta = sectorAngle - angle;
	if (delta < 0) delta += TICKS_PER_ROTATION;
	assert((0 <= delta) && (delta < TICKS_PER_ROTATION));
	//std::cout << "DEBUG a1: " << angle
	//          << " a2: " << sectorAngle
	//          << " delta: " << delta << std::endl;
	
	EmuDuration dur = Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND>::
	                      duration(delta);
	return time + dur;
}

void RealDrive::setHeadLoaded(bool status, const EmuTime& time)
{
	if (headLoadStatus != status) {
		headLoadStatus = status;
		headLoadTimer.advance(time);
	}
}

bool RealDrive::headLoaded(const EmuTime& time)
{
	return headLoadStatus && 
	       (headLoadTimer.getTicksTill(time) > 10);
}

void RealDrive::insertDisk(const string& diskImage, const vector<string>& patches)
{
	ejectDisk();
	
	try {
		// first try XSA
		PRT_DEBUG("Trying an XSA diskimage...");
		disk.reset(new XSADiskImage(diskImage));
		PRT_DEBUG("Succeeded");
	} catch (MSXException& e) {
		try {
			//First try the fake disk, because a DSK will always
			//succeed if diskImage can be resolved 
			//It is simply stat'ed, so even a directory name
			//can be resolved and will be accepted as dsk name
			PRT_DEBUG("Trying a DirAsDSK approach...");
			// try to create fake DSK from a dir on host OS
			disk.reset(new FDC_DirAsDSK(diskImage));
			PRT_DEBUG("Succeeded");
		} catch (MSXException& e) {
			// then try normal DSK
			PRT_DEBUG("Trying a DSK diskimage...");
			disk.reset(new DSKDiskImage(diskImage));
			PRT_DEBUG("Succeeded");
		}
	}
	for (vector<string>::const_iterator it = patches.begin();
	     it != patches.end(); ++it) {
		disk->applyPatch(*it);
	}
	diskElem->setData(diskImage);
}

void RealDrive::ejectDisk()
{
	diskElem->setData("");
	disk.reset(new DummyDisk());
}

string RealDrive::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		const string& diskName = diskElem->getData();
		if (!diskName.empty()) {
			result += "Current disk: " + diskName + '\n';
		} else {
			result += "There is currently no disk inserted in drive with name \"" +
			          name + "\"" + '\n';
		}
	} else if (tokens[1] == "eject") {
		ejectDisk();
		CliCommOutput::instance().update(CliCommOutput::MEDIA,
		                                 name, "");
	} else {
		try {
			UserFileContext context;
			vector<string> patches;
			for (unsigned i = 2; i < tokens.size(); ++i) {
				patches.push_back(context.resolve(tokens[i]));
			}
			insertDisk(context.resolve(tokens[1]), patches);
			diskChangedFlag = true;
			CliCommOutput::instance().update(CliCommOutput::MEDIA,
			                                 name, tokens[1]);
		} catch (FileException &e) {
			throw CommandException(e.getMessage());
		}
	}
	return result;
}

string RealDrive::help(const vector<string>& /*tokens*/) const
{
	return name + " eject      : remove disk from virtual drive\n" +
	       name + " <filename> : change the disk file\n";
}

void RealDrive::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() >= 2)
		CommandController::completeFileName(tokens);
}

bool RealDrive::diskChanged()
{
	bool ret = diskChangedFlag;
	diskChangedFlag = false;
	return ret;
}

bool RealDrive::dummyDrive()
{
	return false;
}


/// class SingleSidedDrive ///

SingleSidedDrive::SingleSidedDrive(const EmuTime& time)
	: RealDrive(time)
{
}

SingleSidedDrive::~SingleSidedDrive()
{
}

bool SingleSidedDrive::doubleSided()
{
	return false;
}

void SingleSidedDrive::setSide(bool /*side*/)
{
	// ignore
}

void SingleSidedDrive::read(byte sector, byte* buf,
                            byte& onDiskTrack, byte& onDiskSector,
                            byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = 0;
	onDiskSize = 512;
	disk->read(headPos, sector, 0, 512, buf);
}

void SingleSidedDrive::write(byte sector, const byte* buf,
                             byte& onDiskTrack, byte& onDiskSector,
                             byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = 0;
	onDiskSize = 512;
	disk->write(headPos, sector, 0, 512, buf);
}

void SingleSidedDrive::getSectorHeader(byte sector, byte* buf)
{
	disk->getSectorHeader(headPos, sector, 0, buf);
}

void SingleSidedDrive::getTrackHeader(byte* buf)
{
	disk->getTrackHeader(headPos, 0, buf);
}

void SingleSidedDrive::initWriteTrack()
{
	disk->initWriteTrack(headPos, 0);
}

void SingleSidedDrive::writeTrackData(byte data)
{
	disk->writeTrackData(data);
}


/// class DoubleSidedDrive ///

DoubleSidedDrive::DoubleSidedDrive(const EmuTime& time)
	: RealDrive(time)
{
	side = 0;
}

DoubleSidedDrive::~DoubleSidedDrive()
{
}

bool DoubleSidedDrive::doubleSided()
{
	return disk->doubleSided();
}

void DoubleSidedDrive::setSide(bool side_)
{
	side = side_ ? 1 : 0;
}

void DoubleSidedDrive::read(byte sector, byte* buf,
                            byte& onDiskTrack, byte& onDiskSector,
                            byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = side;
	onDiskSize = 512;
	disk->read(headPos, sector, side, 512, buf);
}

void DoubleSidedDrive::write(byte sector, const byte* buf,
                             byte& onDiskTrack, byte& onDiskSector,
                             byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = side;
	onDiskSize = 512;
	disk->write(headPos, sector, side, 512, buf);
}

void DoubleSidedDrive::getSectorHeader(byte sector, byte* buf)
{
	disk->getSectorHeader(headPos, sector, side, buf);
}

void DoubleSidedDrive::getTrackHeader(byte* buf)
{
	disk->getTrackHeader(headPos, side, buf);
}

void DoubleSidedDrive::initWriteTrack()
{
	disk->initWriteTrack(headPos, side);
}

void DoubleSidedDrive::writeTrackData(byte data)
{
	disk->writeTrackData(data);
}

} // namespace openmsx
