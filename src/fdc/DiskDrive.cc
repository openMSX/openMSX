// $Id$

#include "DiskDrive.hh"
#include "CommandController.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "DummyDisk.hh"
#include "XSADiskImage.hh"
#include "DSKDiskImage.hh"
#include "FDC_DirAsDSK.hh"
#include "Leds.hh"
#include "FileContext.hh"

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
	throw DriveEmptyException("No drive connected");
}

bool DummyDrive::writeProtected()
{
	return true;
}

bool DummyDrive::doubleSided()
{
	return false;
}

void DummyDrive::setSide(bool side)
{
	// ignore
}

void DummyDrive::step(bool direction, const EmuTime& time)
{
	// ignore
}

bool DummyDrive::track00(const EmuTime& time)
{
	return false; // National_FS-5500F1 2nd drive detection depends on this
}

void DummyDrive::setMotor(bool status, const EmuTime& time)
{
	// ignore
}

bool DummyDrive::indexPulse(const EmuTime& time)
{
	return false;
}

int DummyDrive::indexPulseCount(const EmuTime& begin,
                                const EmuTime& end)
{
	return 0;
}

void DummyDrive::setHeadLoaded(bool status, const EmuTime& time)
{
	// ignore
}

bool DummyDrive::headLoaded(const EmuTime& time)
{
	return false;
}

void DummyDrive::read(byte sector, byte* buf,
                      byte& onDiskTrack, byte& onDiskSector,
                      byte& onDiskSide,  int&  onDiskSize)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::write(byte sector, const byte* buf,
                       byte& onDiskTrack, byte& onDiskSector,
                       byte& onDiskSide,  int& onDiskSize)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::getSectorHeader(byte sector, byte* buf)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::getTrackHeader(byte* buf)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::initWriteTrack()
{
	// ignore ???
}

void DummyDrive::writeTrackData(byte data)
{
	// ignore ???
}

bool DummyDrive::diskChanged()
{
	return false;
}


/// class RealDrive ///

RealDrive::RealDrive(const string& driveName, const EmuTime& time)
{
	headPos = 0;
	motorStatus = false;
	motorTime = time;
	headLoadStatus = false;
	headLoadTime = time;
	
	name = driveName;
	diskName = "";
	disk = NULL;
	diskChangedFlag = false;

	SettingsConfig& conf = SettingsConfig::instance();
	if (conf.hasConfigWithId(driveName)) {
		Config* config = conf.getConfigById(driveName);
		const string& filename = config->getParameter("filename");
		try {
			insertDisk(config->getContext(), filename);
		} catch (FileException &e) {
			// file not found
			throw FatalError("Couldn't load diskimage: " + filename);
		}
	} else {
		// nothing specified
		ejectDisk();
	}
	CommandController::instance().registerCommand(this, name);
}

RealDrive::~RealDrive()
{
	CommandController::instance().unregisterCommand(this, name);
	delete disk;
}

bool RealDrive::ready()
{
	return disk->ready();
}

bool RealDrive::writeProtected()
{
	return disk->writeProtected();
}

void RealDrive::step(bool direction, const EmuTime& time)
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

bool RealDrive::track00(const EmuTime& time)
{
	return headPos == 0;
}

void RealDrive::setMotor(bool status, const EmuTime& time)
{
	if (motorStatus != status) {
		motorStatus = status;
		motorTime = time;
		/* The following is a hack to emulate the drive LED behaviour.
		 * This is in real life dependent on the FDC and should be
		 * investigated in detail to implement it properly... TODO */
		if (motorStatus) {
			Leds::instance().setLed(Leds::FDD_ON); 
		}
		else {
			Leds::instance().setLed(Leds::FDD_OFF);
		}
	}
}

bool RealDrive::indexPulse(const EmuTime& time)
{
	if (!motorStatus && disk->ready()) {
		return false;
	}
	int angle = motorTime.getTicksTill(time) % TICKS_PER_ROTATION;
	return angle < INDEX_DURATION;
}

int RealDrive::indexPulseCount(const EmuTime& begin,
                               const EmuTime& end)
{
	if (!motorStatus && disk->ready()) {
		return 0;
	}
	int t1 = (motorTime <= begin) ? motorTime.getTicksTill(begin) : 0;
	int t2 = (motorTime <= end)   ? motorTime.getTicksTill(end)   : 0;
	return (t2 / TICKS_PER_ROTATION) - (t1 / TICKS_PER_ROTATION);
}

void RealDrive::setHeadLoaded(bool status, const EmuTime& time)
{
	if (headLoadStatus != status) {
		headLoadStatus = status;
		headLoadTime = time;
	}
}

bool RealDrive::headLoaded(const EmuTime& time)
{
	return headLoadStatus && 
	       (headLoadTime.getTicksTill(time) > 10);
}

void RealDrive::insertDisk(FileContext &context,
                           const string& diskImage)
{
	Disk* tmp;
	try {
		// first try XSA
		PRT_DEBUG("Trying an XSA diskimage...");
		tmp = new XSADiskImage(context, diskImage);
		PRT_DEBUG("Succeeded");
	} catch (MSXException &e) {
		try {
			//First try the fake disk, because a DSK will always
			//succeed if diskImage can be resolved 
			//It is simply stat'ed, so even a directory name
			//can be resolved and will be accepted as dsk name
			PRT_DEBUG("Trying a DirAsDSK approach...");
			// try to create fake DSK from a dir on host OS
			tmp = new FDC_DirAsDSK(context, diskImage);
			PRT_DEBUG("Succeeded");
		} catch (MSXException &e) {
			// then try normal DSK
			PRT_DEBUG("Trying a DSK diskimage...");
			tmp = new DSKDiskImage(context, diskImage);
			PRT_DEBUG("Succeeded");
		}
	}
	delete disk;
	disk = tmp;
	diskName = diskImage;
}

void RealDrive::ejectDisk()
{
	PRT_DEBUG("Ejecting disk");
	delete disk;
	diskName = "";
	disk = new DummyDisk();
}

string RealDrive::execute(const vector<string>& tokens)
	throw (CommandException)
{
	string result;
	if (tokens.size() == 1) {
		if (!diskName.empty()) {
			result += "Current disk: " + diskName + '\n';
		} else {
			result += "There is currently no disk inserted in drive with name \"" +
			          name + "\"" + '\n';
		}
	} else if (tokens.size() != 2) {
		throw SyntaxError();
	} else if (tokens[1] == "eject") {
		ejectDisk();
	} else {
		try {
			UserFileContext context;
			insertDisk(context, tokens[1]);
			diskChangedFlag = true;
		} catch (FileException &e) {
			throw CommandException(e.getMessage());
		}
	}
	return result;
}

string RealDrive::help(const vector<string>& tokens) const throw()
{
	return name + " eject      : remove disk from virtual drive\n" +
	       name + " <filename> : change the disk file\n";
}

void RealDrive::tabCompletion(vector<string>& tokens) const throw()
{
	if (tokens.size() == 2)
		CommandController::completeFileName(tokens);
}

bool RealDrive::diskChanged()
{
	bool ret = diskChangedFlag;
	diskChangedFlag = false;
	return ret;
}


/// class SingleSidedDrive ///

SingleSidedDrive::SingleSidedDrive(const string& drivename,
                                   const EmuTime& time)
	: RealDrive(drivename, time)
{
}

SingleSidedDrive::~SingleSidedDrive()
{
}

bool SingleSidedDrive::doubleSided()
{
	return false;
}

void SingleSidedDrive::setSide(bool side)
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

DoubleSidedDrive::DoubleSidedDrive(const string& drivename,
                                   const EmuTime& time)
	: RealDrive(drivename, time)
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

void DoubleSidedDrive::readSector(byte* buf, int sector)
{
	disk->readSector(buf, sector);
}

void DoubleSidedDrive::writeSector(const byte* buf, int sector)
{
	disk->writeSector(buf, sector);
}

} // namespace openmsx
