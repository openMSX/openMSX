// $Id$

#include "IDEHD.hh"
#include "MSXException.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "DiskManipulator.hh"
#include "endian.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

IDEHD::IDEHD(const DeviceConfig& config)
	: HD(config)
	, AbstractIDEDevice(config.getMotherBoard())
	, diskManipulator(config.getReactor().getDiskManipulator())
{
	diskManipulator.registerDrive(*this, config.getMotherBoard().getMachineID() + "::");
}

IDEHD::~IDEHD()
{
	diskManipulator.unregisterDrive(*this);
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
	// TODO use aligned version later
	Endian::write_UA_L16(&buffer[1 * 2], cylinders);
	Endian::write_UA_L16(&buffer[3 * 2], heads);
	Endian::write_UA_L16(&buffer[6 * 2], sectors);

	buffer[47 * 2 + 0] = 16; // max sector transfer per interrupt
	buffer[47 * 2 + 1] = 0x80; // specced value

	// .... 1...: IORDY supported (hardware signal used by PIO modes >3)
	// .... ..1.: LBA supported
	buffer[49 * 2 + 1] = 0x0A;

	Endian::write_UA_L32(&buffer[60 * 2], totalSectors);
}

unsigned IDEHD::readBlockStart(byte* buffer, unsigned count)
{
	try {
		assert(count >= 512);
		(void)count; // avoid warning
		readSector(transferSectorNumber, buffer);
		++transferSectorNumber;
		return 512;
	} catch (MSXException&) {
		abortReadTransfer(UNC);
		return 0;
	}
}

void IDEHD::writeBlockComplete(byte* buffer, unsigned count)
{
	try {
		assert((count % 512) == 0);
		unsigned num = count / 512;
		for (unsigned i = 0; i < num; ++i) {
			writeSector(transferSectorNumber++,
			            buffer + 512 * i);
		}
	} catch (MSXException&) {
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

	case 0xF8: // Read Native Max Address
		// We don't support the Host Protected Area feature set, but SymbOS
		// uses only this particular command, so we support just this one.
		setSectorNumber(getNbSectors());
		break;

	default: // all others
		AbstractIDEDevice::executeCommand(cmd);
	}
}


template<typename Archive>
void IDEHD::serialize(Archive& ar, unsigned /*version*/)
{
	// don't serialize SectorAccessibleDisk, DiskContainer base classes
	ar.template serializeBase<HD>(*this);
	ar.template serializeBase<AbstractIDEDevice>(*this);
	ar.serialize("transferSectorNumber", transferSectorNumber);
}
INSTANTIATE_SERIALIZE_METHODS(IDEHD);
REGISTER_POLYMORPHIC_INITIALIZER(IDEDevice, IDEHD, "IDEHD");

} // namespace openmsx
