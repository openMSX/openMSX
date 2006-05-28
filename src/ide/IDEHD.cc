// $Id$

#include "IDEHD.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXMotherBoard.hh"
#include "FileManipulator.hh"
#include "XMLElement.hh"
#include "Command.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include <cassert>
#include <bitset>

using std::string;
using std::vector;

namespace openmsx {

class HDCommand : public SimpleCommand
{
public:
	HDCommand(CommandController& commandController, IDEHD& hd);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	IDEHD& hd;
};


static const unsigned MAX_HD = 26;
static std::bitset<MAX_HD> hdInUse;

static string calcName()
{
	unsigned id = 0;
	while (hdInUse[id]) {
		++id;
		if (id == MAX_HD) {
			throw MSXException("Too many HDs");
		}
	}
	// for exception safety don't set hdInUse in this routine
	return string("hd") + char('a' + id);
}

IDEHD::IDEHD(MSXMotherBoard& motherBoard, const XMLElement& config,
             const EmuTime& time)
	: AbstractIDEDevice(motherBoard.getEventDistributor(), time)
	, fileManipulator(motherBoard.getFileManipulator())
	, name(calcName())
	, hdCommand(new HDCommand(motherBoard.getCommandController(), *this))
{
	string filename = config.getFileContext().resolveCreate(
		config.getChildData("filename"));
	try {
		file.reset(new File(filename));
	} catch (FileException& e) {
		// image didn't exist yet, create new
		file.reset(new File(filename, File::CREATE));
		file->truncate(config.getChildDataAsInt("size") * 1024 * 1024);
	}

	fileManipulator.registerDrive(*this, name);
	hdInUse[name[2] - 'a'] = true;
}

IDEHD::~IDEHD()
{
	hdInUse[name[2] - 'a'] = false;
	fileManipulator.unregisterDrive(*this, name);
}

bool IDEHD::isPacketDevice()
{
	return false;
}

const std::string& IDEHD::getDeviceName()
{
	static const std::string NAME = "OPENMSX HARD DISK";
	return NAME;
}

void IDEHD::fillIdentifyBlock(byte* buffer)
{
	unsigned totalSectors = getNbSectors();
	word heads = 16;
	word sectors = 32;
	word cylinders = totalSectors / (heads * sectors);
	buffer[1 * 2 + 0] = cylinders & 0xFF;
	buffer[1 * 2 + 1] = cylinders >> 8;
	buffer[3 * 2 + 0] = heads & 0xFF;
	buffer[3 * 2 + 1] = heads >> 8;
	buffer[6 * 2 + 0] = sectors & 0xFF;
	buffer[6 * 2 + 1] = sectors >> 8;

	buffer[47 * 2 + 0] = 16; // max sector transfer per interrupt
	buffer[47 * 2 + 1] = 0x80; // specced value

	// .... 1...: IORDY supported (hardware signal used by PIO modes >3)
	// .... ..1.: LBA supported
	buffer[49 * 2 + 1] = 0x0A;

	buffer[60 * 2 + 0] = (totalSectors & 0x000000FF) >>  0;
	buffer[60 * 2 + 1] = (totalSectors & 0x0000FF00) >>  8;
	buffer[61 * 2 + 0] = (totalSectors & 0x00FF0000) >> 16;
	buffer[61 * 2 + 1] = (totalSectors & 0xFF000000) >> 24;
}

unsigned IDEHD::readBlockStart(byte* buffer, unsigned count)
{
	try {
		assert(count >= 512);
		readLogicalSector(transferSectorNumber, buffer);
		++transferSectorNumber;
		return 512;
	} catch (FileException& e) {
		abortReadTransfer(UNC);
		return 0;
	}
}

void IDEHD::writeBlockComplete(byte* buffer, unsigned count)
{
	try {
		while (count != 0) {
			writeLogicalSector(transferSectorNumber, buffer);
			++transferSectorNumber;
			assert(count >= 512);
			count -= 512;
		}
	} catch (FileException& e) {
		abortWriteTransfer(UNC);
	}
}

void IDEHD::executeCommand(byte cmd)
{
	switch (cmd) {
	case 0x20: // Read Sector
	case 0x30: { // Write Sector
		unsigned sectorNumber = getSectorNumber();
		unsigned numSectors = getNumSectors();
		if ((sectorNumber + numSectors) > getNbSectors()) {
			// Note: The original code set ABORT as well, but that is not
			//       allowed according to the spec.
			setError(IDNF);
			break;
		}
		transferSectorNumber = sectorNumber;
		if (cmd == 0x20) {
			startLongReadTransfer(numSectors * 512);
		} else {
			startWriteTransfer(numSectors * 512);
		}
		break;
	}
	default: // all others
		AbstractIDEDevice::executeCommand(cmd);
	}
}

void IDEHD::readLogicalSector(unsigned sector, byte* buf)
{
	file->seek(512 * sector);
	file->read(buf, 512);
}

void IDEHD::writeLogicalSector(unsigned sector, const byte* buf)
{
	file->seek(512 * sector);
	file->write(buf, 512);
}

unsigned IDEHD::getNbSectors() const
{
	return file->getSize() / 512;
}

SectorAccessibleDisk* IDEHD::getSectorAccessibleDisk()
{
	return this;
}


// class HDCommand

HDCommand::HDCommand(CommandController& commandController, IDEHD& hd_)
	: SimpleCommand(commandController, hd_.name)
	, hd(hd_)
{
}

string HDCommand::execute(const vector<string>& tokens)
{
	switch (tokens.size()) {
	case 1:
		return hd.file->getOriginalName();
	case 2: {
		try {
			CommandController& controller = getCommandController();
			if (controller.getGlobalSettings().
			        getPowerSetting().getValue()) {
				throw CommandException(
					"Can only change hard disk image when MSX "
					"is powered down.");
			}
			UserFileContext context(controller);
			string filename = context.resolve(tokens[1]);
			std::auto_ptr<File> newFile(new File(filename));
			hd.file = newFile;
			return filename;
		} catch (FileException& e) {
			throw CommandException("Can't change hard disk image: " +
			                       e.getMessage());
		}
	}
	default:
		throw CommandException("Too many arguments.");
	}
}

string HDCommand::help(const vector<string>& /*tokens*/) const
{
	return hd.name + ": change the hard disk image for this hard disk drive\n";
}

void HDCommand::tabCompletion(vector<string>& tokens) const
{
	completeFileName(tokens);
}

} // namespace openmsx
