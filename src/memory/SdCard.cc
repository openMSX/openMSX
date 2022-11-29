#include "SdCard.hh"
#include "DeviceConfig.hh"
#include "HD.hh"
#include "MSXException.hh"
#include "unreachable.hh"
#include "endian.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <memory>

// TODO:
// - replace transferDelayCounter with 0xFF's in responseQueue?
// - remove duplication between WRITE and MULTI_WRITE (is it worth it?)
// - see TODOs in the code below

namespace openmsx {

// data response tokens
static constexpr byte DRT_ACCEPTED    = 0x05;
static constexpr byte DRT_WRITE_ERROR = 0x0D;

// start block tokens and stop tran token
static constexpr byte START_BLOCK_TOKEN     = 0xFE;
static constexpr byte START_BLOCK_TOKEN_MBW = 0xFC;
static constexpr byte STOP_TRAN_TOKEN       = 0xFD;

// data error token
static constexpr byte DATA_ERROR_TOKEN_ERROR        = 0x01;
static constexpr byte DATA_ERROR_TOKEN_OUT_OF_RANGE = 0x08;

// responses
static constexpr byte R1_BUSY            = 0x00;
static constexpr byte R1_IDLE            = 0x01; // TODO: why is lots of code checking for this instead of R1_BUSY?
static constexpr byte R1_ILLEGAL_COMMAND = 0x04;
static constexpr byte R1_PARAMETER_ERROR = 0x80;

SdCard::SdCard(const DeviceConfig& config)
	: hd(config.getXML() ? std::make_unique<HD>(config) : nullptr)
{
}

SdCard::~SdCard() = default;

// helper methods for 'transfer' to avoid duplication
byte SdCard::readCurrentByteFromCurrentSector()
{
	byte result = [&] {
		if (currentByteInSector == -1) {
			try {
				hd->readSector(currentSector, sectorBuf);
				return START_BLOCK_TOKEN;
			} catch (MSXException&) {
				return DATA_ERROR_TOKEN_ERROR;
			}
		} else {
			// output next byte from stream
			return sectorBuf.raw[currentByteInSector];
		}
	}();
	currentByteInSector++;
	if (currentByteInSector == sizeof(sectorBuf)) {
		responseQueue.push_back({byte(0x00), byte(0x00)}); // 2 CRC's (dummy)
	}
	return result;
}

byte SdCard::transfer(byte value, bool cs)
{
	if (!hd) return 0xFF; // no card inserted

	if (cs) {
		// /CS is true: not for this chip
		return 0xFF;
	}

	// process output
	byte retval = 0xFF;
	if (transferDelayCounter > 0) {
		--transferDelayCounter;
	} else {
		if (responseQueue.empty()) {
			switch (mode) {
			case READ:
				retval = readCurrentByteFromCurrentSector();
				if (currentByteInSector == sizeof(sectorBuf)) {
					mode = COMMAND;
				}
				break;
			case MULTI_READ:
				// when there's an error, you probably have to send a CMD12
				// to go back to the COMMAND mode. This is not
				// clear in the spec (it's wrongly suggested
				// for the MULTI_WRITE mode!)
				if (currentSector >= hd->getNbSectors()) {
					// data out of range, send data error token
					retval = DATA_ERROR_TOKEN_OUT_OF_RANGE;
				} else {
					retval = readCurrentByteFromCurrentSector();
					if (currentByteInSector == sizeof(sectorBuf)) {
						currentSector++;
						currentByteInSector = -1;
					}
				}
				break;
			case WRITE:
			case MULTI_WRITE:
				// apparently nothing is returned while writing
			case COMMAND:
			default:
			break;
			}
		} else {
			retval = responseQueue.pop_front();
		}
	}

	// process input
	switch (mode) {
	case WRITE:
		// first check for data token
		if (currentByteInSector == -1) {
			if (value == START_BLOCK_TOKEN) {
				currentByteInSector++;
			}
			break;
		}
		if (currentByteInSector < int(sizeof(sectorBuf))) {
			sectorBuf.raw[currentByteInSector] = value;
		}
		currentByteInSector++;
		if (currentByteInSector == (sizeof(sectorBuf) + 2)) {
			byte response = DRT_ACCEPTED;
			// copy buffer to SD card
			try {
				hd->writeSector(currentSector, sectorBuf);
			} catch (MSXException&) {
				response = DRT_WRITE_ERROR;
			}
			mode = COMMAND;
			transferDelayCounter = 1;
			responseQueue.push_back(response);
		}
		break;
	case MULTI_WRITE:
		// first check for stop or start token
		if (currentByteInSector == -1) {
			if (value == STOP_TRAN_TOKEN) {
				mode = COMMAND;
			}
			if (value == START_BLOCK_TOKEN_MBW) {
				currentByteInSector++;
			}
			break;
		}
		if (currentByteInSector < int(sizeof(sectorBuf))) {
			sectorBuf.raw[currentByteInSector] = value;
		}
		currentByteInSector++;
		if (currentByteInSector == (sizeof(sectorBuf) + 2)) {
			// check if still in valid range
			byte response = DRT_ACCEPTED;
			if (currentSector >= hd->getNbSectors()) {
				response = DRT_WRITE_ERROR;
				// note: mode is not changed, should be done by
				// the host with CMD12 (STOP_TRANSMISSION) -
				// however, this makes no sense, CMD12 is only
				// for Multiple Block Read!? Wrong in the spec?
			} else {
				// copy buffer to SD card
				try {
					hd->writeSector(currentSector, sectorBuf);
					currentByteInSector = -1;
					currentSector++;
				} catch (MSXException&) {
					response = DRT_WRITE_ERROR;
					// note: mode is not changed, should be
					// done by the host with CMD12
					// (STOP_TRANSMISSION) - however, this
					// makes no sense, CMD12 is only for
					// Multiple Block Read!? Wrong in the
					// spec?
				}
			}
			transferDelayCounter = 1;
			responseQueue.push_back(response);
		}
		break;
	case COMMAND:
	default:
		if ((cmdIdx == 0 && (value >> 6) == 1) // command start
				|| cmdIdx > 0) { // command in progress
			cmdBuf[cmdIdx] = value;
			++cmdIdx;
			if (cmdIdx == 6) {
				executeCommand();
				cmdIdx = 0;
			}
		}
		break;
	}

	return retval;
}

void SdCard::executeCommand()
{
	// it takes 2 transfers (2x8 cycles) before a reply
	// can be given to a command
	transferDelayCounter = 2;
	byte command = cmdBuf[0] & 0x3F;
	switch (command) {
	case 0:  // GO_IDLE_STATE
		responseQueue.clear();
		mode = COMMAND;
		responseQueue.push_back(R1_IDLE);
		break;
	case 8:  // SEND_IF_COND
		// conditions are always OK
		responseQueue.push_back({
			R1_IDLE,    // R1 (OK) SDHC (checked by MegaSD and FUZIX)
			byte(0x02), // command version
			byte(0x00), // reserved
			byte(0x01), // voltage accepted
			cmdBuf[4]});// check pattern
		break;
	case 9:{ // SEND_CSD
		responseQueue.push_back({
			R1_BUSY, // OK (ignored on MegaSD code, used in FUZIX)
		// now follows a CSD version 2.0 (for SDHC)
			START_BLOCK_TOKEN, // data token
			byte(0x40),        // CSD_STRUCTURE [127:120]
			byte(0x0E),        // (TAAC)
			byte(0x00),        // (NSAC)
			byte(0x32),        // (TRAN_SPEED)
			byte(0x00),        // CCC
			byte(0x00),        // CCC / (READ_BL_LEN)
			byte(0x00)});      // (RBP)/(WBM)/(RBM)/ DSR_IMP
		// SD_CARD_SIZE = (C_SIZE + 1) * 512kByte
		auto c_size = narrow<uint32_t>((hd->getNbSectors() * sizeof(sectorBuf)) / size_t(512 * 1024) - 1);
		responseQueue.push_back({
			byte((c_size >> 16) & 0x3F), // C_SIZE 1
			byte((c_size >>  8) & 0xFF), // C_SIZE 2
			byte((c_size >>  0) & 0xFF), // C_SIZE 3
			byte(0x00),   // res/(EBE)/(SS1)
			byte(0x00),   // (SS2)/(WGS)
			byte(0x00),   // (WGE)/res/(RF)/(WBL1)
			byte(0x00),   // (WBL2)/(WBP)/res
			byte(0x00),   // (FFG)/COPY/PWP/TWP/(FF)/res
			byte(0x01)}); // CRC / 1
		break;}
	case 10: // SEND_CID
		responseQueue.push_back({
			R1_BUSY, // OK (ignored on MegaSD, unused in FUZIX so far)
			START_BLOCK_TOKEN, // data token
			byte(0xAA),   // CID01 // manuf ID
			byte('o' ),   // CID02 // OEM/App ID 1
			byte('p' ),   // CID03 // OEM/App ID 2
			byte('e' ),   // CID04 // Prod name 1
			byte('n' ),   // CID05 // Prod name 2
			byte('M' ),   // CID06 // Prod name 3
			byte('S' ),   // CID07 // Prod name 4
			byte('X' ),   // CID08 // Prod name 5
			byte(0x01),   // CID09 // Prod Revision
			byte(0x12),   // CID10 // Prod Serial 1
			byte(0x34),   // CID11 // Prod Serial 2
			byte(0x56),   // CID12 // Prod Serial 3
			byte(0x78),   // CID13 // Prod Serial 4
			byte(0x00),   // CID14 // reserved / Y1
			byte(0xE6),   // CID15 // Y2 / M
			byte(0x01)}); // CID16 // CRC / not used
		break;
	case 12: // STOP TRANSMISSION
		responseQueue.push_back(R1_IDLE); // R1 (OK)
		mode = COMMAND;
		break;
	case 16: // SET_BLOCK_LEN
		responseQueue.push_back(R1_IDLE); // OK, we don't really care
		break;
	case 17: // READ_SINGLE_BLOCK
	case 18: // READ_MULTIPLE_BLOCK
	case 24: // WRITE_BLOCK
	case 25: // WRITE_MULTIPLE_BLOCK
		// SDHC so the address is the sector
		currentSector = Endian::readB32(&cmdBuf[1]);
		if (currentSector >= hd->getNbSectors()) {
			responseQueue.push_back(R1_PARAMETER_ERROR);
		} else {
			responseQueue.push_back(R1_BUSY);
			switch (command) {
				case 17: mode = READ; break;
				case 18: mode = MULTI_READ; break;
				case 24: mode = WRITE; break;
				case 25: mode = MULTI_WRITE; break;
				default: UNREACHABLE;
			}
			currentByteInSector = -1; // wait for token
		}
		break;
	case 41: // implementation of ACMD 41!!
		 // SD_SEND_OP_COND
		responseQueue.push_back(R1_BUSY);
		break;
	case 55: // APP_CMD
		// TODO: go to ACMD mode, but not necessary now
		responseQueue.push_back(R1_IDLE);
		break;
	case 58: // READ_OCR
		responseQueue.push_back({
			R1_BUSY,// R1 (OK) (ignored on MegaSD, checked in FUZIX)
			byte(0x40),   // OCR Reg part 1 (SDHC: CCS=1)
			byte(0x00),   // OCR Reg part 2
			byte(0x00),   // OCR Reg part 3
			byte(0x00)}); // OCR Reg part 4
		break;

	default:
		responseQueue.push_back(R1_ILLEGAL_COMMAND);
		break;
	}
}

static constexpr std::initializer_list<enum_string<SdCard::Mode>> modeInfo = {
	{ "COMMAND",     SdCard::COMMAND  },
	{ "READ",        SdCard::READ },
	{ "MULTI_READ",  SdCard::MULTI_READ },
	{ "WRITE",       SdCard::WRITE },
	{ "MULTI_WRITE", SdCard::MULTI_WRITE }
};
SERIALIZE_ENUM(SdCard::Mode, modeInfo);

template<typename Archive>
void SdCard::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mode",   mode,
	             "cmdBuf", cmdBuf);
	ar.serialize_blob("sectorBuf", sectorBuf.raw);
	if (hd) ar.serialize("hd", *hd);
	ar.serialize("cmdIdx",               cmdIdx,
	             "transferDelayCounter", transferDelayCounter,
	             "responseQueue",        responseQueue,
	             "currentSector",        currentSector,
	             "currentByteInSector",  currentByteInSector);
}
INSTANTIATE_SERIALIZE_METHODS(SdCard);

} // namespace openmsx
