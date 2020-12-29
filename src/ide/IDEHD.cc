#include "IDEHD.hh"
#include "MSXException.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "DiskManipulator.hh"
#include "endian.hh"
#include "serialize.hh"
#include "strCat.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

IDEHD::IDEHD(const DeviceConfig& config)
	: HD(config)
	, AbstractIDEDevice(config.getMotherBoard())
	, diskManipulator(config.getReactor().getDiskManipulator())
{
	transferSectorNumber = 0; // avoid UMR is serialize()
	diskManipulator.registerDrive(
		*this, tmpStrCat(config.getMotherBoard().getMachineID(), "::"));
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

void IDEHD::fillIdentifyBlock(AlignedBuffer& buf)
{
	auto totalSectors = getNbSectors();
	uint16_t heads = 16;
	uint16_t sectors = 32;
	auto cylinders = uint16_t(totalSectors / (heads * sectors)); // TODO overflow?
	Endian::writeL16(&buf[1 * 2], cylinders);
	Endian::writeL16(&buf[3 * 2], heads);
	Endian::writeL16(&buf[6 * 2], sectors);

	buf[47 * 2 + 0] = 16; // max sector transfer per interrupt
	buf[47 * 2 + 1] = 0x80; // specced value

	// .... 1...: IORDY supported (hardware signal used by PIO modes >3)
	// .... ..1.: LBA supported
	buf[49 * 2 + 1] = 0x0A;

	// TODO check for overflow
	Endian::writeL32(&buf[60 * 2], unsigned(totalSectors));
}

unsigned IDEHD::readBlockStart(AlignedBuffer& buf, unsigned count)
{
	try {
		assert(count >= 512);
		(void)count; // avoid warning
		readSector(transferSectorNumber,
		           *aligned_cast<SectorBuffer*>(buf));
		++transferSectorNumber;
		return 512;
	} catch (MSXException&) {
		abortReadTransfer(UNC);
		return 0;
	}
}

void IDEHD::writeBlockComplete(AlignedBuffer& buf, unsigned count)
{
	try {
		assert((count % 512) == 0);
		unsigned num = count / 512;
		for (auto i : xrange(num)) {
			writeSector(transferSectorNumber++,
			            *aligned_cast<SectorBuffer*>(buf + 512 * i));
		}
	} catch (MSXException&) {
		abortWriteTransfer(UNC);
	}
}

void IDEHD::executeCommand(byte cmd)
{
	if (0x10 <= cmd && cmd < 0x20) {
		// Recalibrate
		setError(0);
		setByteCount(0);
		return;
	}
	switch (cmd) {
	case 0x20: // Read Sector
	case 0x21: // Read Sector without Retry
	case 0x30: // Write Sector
	case 0x31: { // Write Sector without Retry
		unsigned sectorNumber = getSectorNumber();
		unsigned numSectors = getNumSectors();
		if ((sectorNumber + numSectors) > getNbSectors()) {
			// Note: The original code set ABORT as well, but that is not
			//       allowed according to the spec.
			setError(IDNF);
			break;
		}
		transferSectorNumber = sectorNumber;
		if (cmd < 0x30) {
			startLongReadTransfer(numSectors * 512);
		} else {
			startWriteTransfer(numSectors * 512);
		}
		break;
	}

	case 0xF8: // Read Native Max Address
		// We don't support the Host Protected Area feature set, but SymbOS
		// uses only this particular command, so we support just this one.
		// TODO this only supports 28-bit sector numbers
		setSectorNumber(unsigned(getNbSectors()));
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
