// $Id$

#include "DiskDrive.hh"
#include "CommandController.hh"
#include "MSXConfig.hh"
#include "FDCDummyBackEnd.hh"
#include "FDC_DSK.hh"
#include "FDC_XSA.hh"


/// class DiskDrive ///

DiskDrive::~DiskDrive()
{
}

void DiskDrive::readSector(byte* buf, int sector)
{
	assert(false);
}

void DiskDrive::writeSector(const byte* buf, int sector)
{
	assert(false);
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
	return false;	// TODO check
}

bool DummyDrive::writeProtected()
{
	return true;	// TODO check
}

bool DummyDrive::doubleSided()
{
	return false;	// TODO check
}

void DummyDrive::setSide(bool side)
{
	// ignore
}

void DummyDrive::step(bool direction, const EmuTime &time)
{
	// ignore
}

bool DummyDrive::track00(const EmuTime &time)
{
	return false;	// TODO check
}

void DummyDrive::setMotor(bool status, const EmuTime &time)
{
	// ignore
}

bool DummyDrive::indexPulse(const EmuTime &time)
{
	return false;	// TODO check
}

int DummyDrive::indexPulseCount(const EmuTime &begin,
                                const EmuTime &end)
{
	return 0;
}

void DummyDrive::setHeadLoaded(bool status, const EmuTime &time)
{
	// ignore
}

bool DummyDrive::headLoaded(const EmuTime &time)
{
	return false;	// TODO check
}

void DummyDrive::read(byte sector, int size, byte* buf)
{
}
	// ignore

void DummyDrive::write(byte sector, int size, const byte* buf)
{
	// ignore
}

void DummyDrive::getSectorHeader(byte sector, byte* buf)
{
	// ignore
}

void DummyDrive::getTrackHeader(byte track, byte* buf)
{
	// ignore
}

void DummyDrive::initWriteTrack(byte track)
{
	// ignore
}

void DummyDrive::writeTrackData(byte data)
{
	// ignore
}


/// class RealDrive ///

RealDrive::RealDrive(const std::string &driveName, const EmuTime &time)
{
	headPos = 0;
	motorStatus = false;
	motorTime = time;
	headLoadStatus = false;
	headLoadTime = time;
	
	name = driveName;
	disk = NULL;
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->
			getConfigById(driveName);
		std::string disk = config->getParameter("filename");
		insertDisk(disk);
	} catch (MSXException &e) {
		// nothing specified or file not found
		ejectDisk();
	}
	CommandController::instance()->registerCommand(*this, name);
}

RealDrive::~RealDrive()
{
	CommandController::instance()->unregisterCommand(name);
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

void RealDrive::step(bool direction, const EmuTime &time)
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
}

bool RealDrive::track00(const EmuTime &time)
{
	return headPos == 0;
}

void RealDrive::setMotor(bool status, const EmuTime &time)
{
	motorStatus = status;
	motorTime = time;
}

bool RealDrive::indexPulse(const EmuTime &time)
{
	int angle = motorTime.getTicksTill(time) % TICKS_PER_ROTATION;
	return angle < INDEX_DURATION;
}

int RealDrive::indexPulseCount(const EmuTime &begin,
                               const EmuTime &end)
{
	assert(motorTime <= begin);
	assert(motorTime <= end);
	int t1 = motorTime.getTicksTill(begin);
	int t2 = motorTime.getTicksTill(end);
	int total = t2 - t1;
	int start = t1 % TICKS_PER_ROTATION;
	return (total - start) / TICKS_PER_ROTATION;
}

void RealDrive::setHeadLoaded(bool status, const EmuTime &time)
{
	headLoadStatus = status;
	headLoadTime = time;
}

bool RealDrive::headLoaded(const EmuTime &time)
{
	return headLoadStatus && 
	       (headLoadTime.getTicksTill(time) > 10);
}

void RealDrive::insertDisk(const std::string &diskImage)
{
	FDCBackEnd* tmp;
	try {
		// first try XSA
		tmp = new FDC_XSA(diskImage);
	} catch (MSXException &e) {
		// if that fails use DSK
		tmp = new FDC_DSK(diskImage);
	}
	delete disk;
	disk = tmp;
}

void RealDrive::ejectDisk()
{
	delete disk;
	disk = new FDCDummyBackEnd();
}

void RealDrive::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size() != 2)
		throw CommandException("Syntax error");
	if (tokens[1] == "eject") {
		ejectDisk();
	} else {
		try {
			insertDisk(tokens[1]);
		} catch (MSXException &e) {
			throw CommandException(e.desc);
		}
	}
}

void RealDrive::help(const std::vector<std::string> &tokens)
{
	print(name + " eject      : remove disk from virtual drive");
	print(name + " <filename> : change the disk file");
}

void RealDrive::tabCompletion(std::vector<std::string> &tokens)
{
	if (tokens.size() == 2)
		CommandController::completeFileName(tokens);
}


/// class SingleSidedDrive ///

SingleSidedDrive::SingleSidedDrive(const std::string &drivename,
                                   const EmuTime &time)
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

void SingleSidedDrive::read(byte sector, int size, byte* buf)
{
	disk->read(headPos, headPos, sector, 0, size, buf);
}

void SingleSidedDrive::write(byte sector, int size, const byte* buf)
{
	disk->write(headPos, headPos, sector, 0, size, buf);
}

void SingleSidedDrive::getSectorHeader(byte sector, byte* buf)
{
	disk->getSectorHeader(headPos, headPos, sector, 0, buf);
}

void SingleSidedDrive::getTrackHeader(byte track, byte* buf)
{
	disk->getTrackHeader(headPos, headPos, 0, buf);
}

void SingleSidedDrive::initWriteTrack(byte track)
{
	disk->initWriteTrack(headPos, track, 0);
}

void SingleSidedDrive::writeTrackData(byte data)
{
	disk->writeTrackData(data);
}


/// class DoubleSidedDrive ///

DoubleSidedDrive::DoubleSidedDrive(const std::string &drivename,
                                   const EmuTime &time)
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

void DoubleSidedDrive::read(byte sector, int size, byte* buf)
{
	disk->read(headPos, headPos, sector, side, size, buf);
}

void DoubleSidedDrive::write(byte sector, int size, const byte* buf)
{
	disk->write(headPos, headPos, sector, side, size, buf);
}

void DoubleSidedDrive::getSectorHeader(byte sector, byte* buf)
{
	disk->getSectorHeader(headPos, headPos, sector, side, buf);
}

void DoubleSidedDrive::getTrackHeader(byte track, byte* buf)
{
	disk->getTrackHeader(headPos, headPos, side, buf);
}

void DoubleSidedDrive::initWriteTrack(byte track)
{
	disk->initWriteTrack(headPos, track, side);
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
