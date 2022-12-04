/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/MB89352.c,v
** Revision: 1.9
** Date: 2007/03/28 17:35:35
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/
/*
 * Notes:
 *  Not support padding transfer and interrupt signal. (Not used MEGA-SCSI)
 *  Message system might be imperfect. (Not used in MEGA-SCSI usually)
 */
#include "MB89352.hh"
#include "SCSIDevice.hh"
#include "DummySCSIDevice.hh"
#include "SCSIHD.hh"
#include "SCSILS120.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "enumerate.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>
#include <memory>

namespace openmsx {

static constexpr uint8_t REG_BDID =  0;   // Bus Device ID        (r/w)
static constexpr uint8_t REG_SCTL =  1;   // Spc Control          (r/w)
static constexpr uint8_t REG_SCMD =  2;   // Command              (r/w)
static constexpr uint8_t REG_OPEN =  3;   //                      (open)
static constexpr uint8_t REG_INTS =  4;   // Interrupt Sense      (r/w)
static constexpr uint8_t REG_PSNS =  5;   // Phase Sense          (r)
static constexpr uint8_t REG_SDGC =  5;   // SPC Diag. Control    (w)
static constexpr uint8_t REG_SSTS =  6;   // SPC SCSI::STATUS           (r)
static constexpr uint8_t REG_SERR =  7;   // SPC Error SCSI::STATUS     (r/w?)
static constexpr uint8_t REG_PCTL =  8;   // Phase Control        (r/w)
static constexpr uint8_t REG_MBC  =  9;   // Modified Byte Counter(r)
static constexpr uint8_t REG_DREG = 10;   // Data Register        (r/w)
static constexpr uint8_t REG_TEMP = 11;   // Temporary Register   (r/w)
                                          // Another value is maintained respectively
                                          // for writing and for reading
static constexpr uint8_t REG_TCH  = 12;   // Transfer Counter High(r/w)
static constexpr uint8_t REG_TCM  = 13;   // Transfer Counter Mid (r/w)
static constexpr uint8_t REG_TCL  = 14;   // Transfer Counter Low (r/w)

static constexpr uint8_t REG_TEMPWR = 13; // (TEMP register preservation place for writing)
static constexpr uint8_t FIX_PCTL   = 14; // (REG_PCTL & 7)

static constexpr uint8_t PSNS_IO  = 0x01;
static constexpr uint8_t PSNS_CD  = 0x02;
static constexpr uint8_t PSNS_MSG = 0x04;
static constexpr uint8_t PSNS_BSY = 0x08;
static constexpr uint8_t PSNS_SEL = 0x10;
static constexpr uint8_t PSNS_ATN = 0x20;
static constexpr uint8_t PSNS_ACK = 0x40;
static constexpr uint8_t PSNS_REQ = 0x80;

static constexpr uint8_t PSNS_SELECTION = PSNS_SEL;
static constexpr uint8_t PSNS_COMMAND   = PSNS_CD;
static constexpr uint8_t PSNS_DATAIN    = PSNS_IO;
static constexpr uint8_t PSNS_DATAOUT   = 0;
static constexpr uint8_t PSNS_STATUS    = PSNS_CD  | PSNS_IO;
static constexpr uint8_t PSNS_MSGIN     = PSNS_MSG | PSNS_CD | PSNS_IO;
static constexpr uint8_t PSNS_MSGOUT    = PSNS_MSG | PSNS_CD;

static constexpr uint8_t INTS_ResetCondition  = 0x01;
static constexpr uint8_t INTS_SPC_HardError   = 0x02;
static constexpr uint8_t INTS_TimeOut         = 0x04;
static constexpr uint8_t INTS_ServiceRequited = 0x08;
static constexpr uint8_t INTS_CommandComplete = 0x10;
static constexpr uint8_t INTS_Disconnected    = 0x20;
static constexpr uint8_t INTS_ReSelected      = 0x40;
static constexpr uint8_t INTS_Selected        = 0x80;

static constexpr uint8_t CMD_BusRelease    = 0x00;
static constexpr uint8_t CMD_Select        = 0x20;
static constexpr uint8_t CMD_ResetATN      = 0x40;
static constexpr uint8_t CMD_SetATN        = 0x60;
static constexpr uint8_t CMD_Transfer      = 0x80;
static constexpr uint8_t CMD_TransferPause = 0xA0;
static constexpr uint8_t CMD_Reset_ACK_REQ = 0xC0;
static constexpr uint8_t CMD_Set_ACK_REQ   = 0xE0;
static constexpr uint8_t CMD_MASK          = 0xE0;

MB89352::MB89352(const DeviceConfig& config)
{
	// TODO: devBusy = false;

	// ALMOST COPY PASTED FROM WD33C93:

	for (const auto* t : config.getXML()->getChildren("target")) {
		unsigned id = t->getAttributeValueAsInt("id", 0);
		if (id >= MAX_DEV) {
			throw MSXException(
				"Invalid SCSI id: ", id,
				" (should be 0..", MAX_DEV - 1, ')');
		}
		if (dev[id]) {
			throw MSXException("Duplicate SCSI id: ", id);
		}
		DeviceConfig conf(config, *t);
		auto type = t->getChildData("type");
		if (type == "SCSIHD") {
			dev[id] = std::make_unique<SCSIHD>(conf, buffer,
			        SCSIDevice::MODE_SCSI2 | SCSIDevice::MODE_MEGASCSI);
		} else if (type == "SCSILS120") {
			dev[id] = std::make_unique<SCSILS120>(conf, buffer,
			        SCSIDevice::MODE_SCSI2 | SCSIDevice::MODE_MEGASCSI);
		} else {
			throw MSXException("Unknown SCSI device: ", type);
		}
	}
	// fill remaining targets with dummy SCSI devices to prevent crashes
	for (auto& d : dev) {
		if (!d) d = std::make_unique<DummySCSIDevice>();
	}
	reset(false);

	// avoid UMR on savestate
	ranges::fill(buffer, 0);
}

void MB89352::disconnect()
{
	if (phase != SCSI::BUS_FREE) {
		assert(targetId < MAX_DEV);
		dev[targetId]->disconnect();
		regs[REG_INTS] |= INTS_Disconnected;
		phase      = SCSI::BUS_FREE;
		nextPhase  = SCSI::UNDEFINED;
	}

	regs[REG_PSNS] = 0;
	isBusy         = false;
	isTransfer     = false;
	counter        = 0;
	tc             = 0;
	atn            = 0;
}

void MB89352::softReset()
{
	isEnabled = false;

	for (auto i : xrange(2, 15)) {
		regs[i] = 0;
	}
	regs[15] = 0xFF;               // un mapped
	ranges::fill(cdb, 0);

	cdbIdx = 0;
	bufIdx = 0;
	phase  = SCSI::BUS_FREE;
	disconnect();
}

void MB89352::reset(bool scsiReset)
{
	regs[REG_BDID] = 0x80;     // Initial value
	regs[REG_SCTL] = 0x80;
	rst  = false;
	atn  = 0;
	myId = 7;

	softReset();

	if (scsiReset) {
		for (auto& d : dev) {
			d->reset();
		}
	}
}

void MB89352::setACKREQ(uint8_t& value)
{
	// REQ check
	if ((regs[REG_PSNS] & (PSNS_REQ | PSNS_BSY)) != (PSNS_REQ | PSNS_BSY)) {
		// set ACK/REQ: REQ/BSY check error
		if (regs[REG_PSNS] & PSNS_IO) { // SCSI -> SPC
			value = 0xFF;
		}
		return;
	}

	// phase check
	if (regs[FIX_PCTL] != (regs[REG_PSNS] & 7)) {
		// set ACK/REQ: phase check error
		if (regs[REG_PSNS] & PSNS_IO) { // SCSI -> SPC
			value = 0xFF;
		}
		if (isTransfer) {
			regs[REG_INTS] |= INTS_ServiceRequited;
		}
		return;
	}

	switch (phase) {
	case SCSI::DATA_IN: // Transfer phase (data in)
		value = buffer[bufIdx];
		++bufIdx;
		regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_DATAIN;
		break;

	case SCSI::DATA_OUT: // Transfer phase (data out)
		buffer[bufIdx] = value;
		++bufIdx;
		regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_DATAOUT;
		break;

	case SCSI::COMMAND: // Command phase
		if (counter < 0) {
			// Initialize command routine
			cdbIdx  = 0;
			counter = (value < 0x20) ? 6 : ((value < 0xA0) ? 10 : 12);
		}
		cdb[cdbIdx] = value;
		++cdbIdx;
		regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_COMMAND;
		break;

	case SCSI::STATUS: // SCSI::STATUS phase
		value = dev[targetId]->getStatusCode();
		regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_STATUS;
		break;

	case SCSI::MSG_IN: // Message In phase
		value = dev[targetId]->msgIn();
		regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_MSGIN;
		break;

	case SCSI::MSG_OUT: // Message Out phase
		msgin |= dev[targetId]->msgOut(value);
		regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_MSGOUT;
		break;

	default:
		// set ACK/REQ code error
		break;
	}
}

void MB89352::resetACKREQ()
{
	// ACK check
	if ((regs[REG_PSNS] & (PSNS_ACK | PSNS_BSY)) != (PSNS_ACK | PSNS_BSY)) {
		// reset ACK/REQ: ACK/BSY check error
		return;
	}

	// phase check
	if (regs[FIX_PCTL] != (regs[REG_PSNS] & 7)) {
		// reset ACK/REQ: phase check error
		if (isTransfer) {
			regs[REG_INTS] |= INTS_ServiceRequited;
		}
		return;
	}

	switch (phase) {
	case SCSI::DATA_IN: // Transfer phase (data in)
		if (--counter > 0) {
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAIN;
		} else {
			if (blockCounter > 0) {
				counter = narrow<int>(dev[targetId]->dataIn(blockCounter));
				if (counter) {
					regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAIN;
					bufIdx = 0;
					break;
				}
			}
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_STATUS;
			phase = SCSI::STATUS;
		}
		break;

	case SCSI::DATA_OUT: // Transfer phase (data out)
		if (--counter > 0) {
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
		} else {
			counter = narrow<int>(dev[targetId]->dataOut(blockCounter));
			if (counter) {
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
				bufIdx = 0;
				break;
			}
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_STATUS;
			phase = SCSI::STATUS;
		}
		break;

	case SCSI::COMMAND: // Command phase
		if (--counter > 0) {
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_COMMAND;
		} else {
			bufIdx = 0; // reset buffer index
			// TODO: devBusy = true;
			counter = narrow<int>(dev[targetId]->executeCmd(cdb, phase, blockCounter));
			switch (phase) {
			case SCSI::DATA_IN:
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAIN;
				break;
			case SCSI::DATA_OUT:
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
				break;
			case SCSI::STATUS:
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_STATUS;
				break;
			case SCSI::EXECUTE:
				regs[REG_PSNS] = PSNS_BSY;
				return; // note: return iso break
			default:
				// phase error
				break;
			}
			// TODO: devBusy = false;
		}
		break;

	case SCSI::STATUS: // SCSI::STATUS phase
		regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGIN;
		phase = SCSI::MSG_IN;
		break;

	case SCSI::MSG_IN: // Message In phase
		if (msgin <= 0) {
			disconnect();
			break;
		}
		msgin = 0;
		[[fallthrough]];
	case SCSI::MSG_OUT: // Message Out phase
		if (msgin == -1) {
			disconnect();
			return;
		}

		if (atn) {
			if (msgin & 2) {
				disconnect();
				return;
			}
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGOUT;
			return;
		}

		if (msgin & 1) {
			phase = SCSI::MSG_IN;
		} else {
			if (msgin & 4) {
				phase = SCSI::STATUS;
				nextPhase = SCSI::UNDEFINED;
			} else {
				phase = nextPhase;
				nextPhase = SCSI::UNDEFINED;
			}
		}

		msgin = 0;

		switch (phase) {
		case SCSI::COMMAND:
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_COMMAND;
			break;
		case SCSI::DATA_IN:
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAIN;
			break;
		case SCSI::DATA_OUT:
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
			break;
		case SCSI::STATUS:
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_STATUS;
			break;
		case SCSI::MSG_IN:
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGIN;
			break;
		default:
			// MsgOut code error
			break;
		}
		return;

	default:
		//UNREACHABLE;
		// reset ACK/REQ code error
		break;
	}

	if (atn) {
		nextPhase = phase;
		phase = SCSI::MSG_OUT;
		regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGOUT;
	}
}

uint8_t MB89352::readDREG()
{
	if (isTransfer && (tc > 0)) {
		setACKREQ(regs[REG_DREG]);
		resetACKREQ();

		--tc;
		if (tc == 0) {
			isTransfer = false;
			regs[REG_INTS] |= INTS_CommandComplete;
		}
		regs[REG_MBC] = (regs[REG_MBC] - 1) & 0x0F;
		return regs[REG_DREG];
	} else {
		return 0xFF;
	}
}

void MB89352::writeDREG(uint8_t value)
{
	if (isTransfer && (tc > 0)) {
		setACKREQ(value);
		resetACKREQ();

		--tc;
		if (tc == 0) {
			isTransfer = false;
			regs[REG_INTS] |= INTS_CommandComplete;
		}
		regs[REG_MBC] = (regs[REG_MBC] - 1) & 0x0F;
	}
}

void MB89352::writeRegister(uint8_t reg, uint8_t value)
{
	switch (reg) {
	case REG_DREG: // write data Register
		writeDREG(value);
		break;

	case REG_SCMD: {
		if (!isEnabled) {
			break;
		}

		// bus reset
		if (value & 0x10) {
			if (((regs[REG_SCMD] & 0x10) == 0) && (regs[REG_SCTL] == 0)) {
				rst = true;
				regs[REG_INTS] |= INTS_ResetCondition;
				for (auto& d : dev) {
					d->busReset();
				}
				disconnect();  // alternative routine
			}
		} else {
			rst = false;
		}

		regs[REG_SCMD] = value;

		// execute spc command
		switch (value & CMD_MASK) {
		case CMD_Set_ACK_REQ:
			switch (phase) {
			case SCSI::DATA_IN:
			case SCSI::STATUS:
			case SCSI::MSG_IN:
				setACKREQ(regs[REG_TEMP]);
				break;
			default:
				setACKREQ(regs[REG_TEMPWR]);
			}
			break;

		case CMD_Reset_ACK_REQ:
			resetACKREQ();
			break;

		case CMD_Select: {
			if (rst) {
				regs[REG_INTS] |= INTS_TimeOut;
				break;
			}

			if (regs[REG_PCTL] & 1) {
				// reselection error
				regs[REG_INTS] |= INTS_TimeOut;
				disconnect();
				break;
			}
			bool err = false;
			int x = regs[REG_BDID] & regs[REG_TEMPWR];
			if (phase == SCSI::BUS_FREE && x && x != regs[REG_TEMPWR]) {
				x = regs[REG_TEMPWR] & ~regs[REG_BDID];
				assert(x != 0); // because of the check 2 lines above

				// the targetID is calculated.
				// It is given priority that the number is large.
				targetId = 0;
				while (true) {
					x >>= 1;
					if (x == 0) break;
					++targetId;
					assert(targetId < MAX_DEV);
				}

				if (/*!TODO: devBusy &&*/ dev[targetId]->isSelected()) {
					// target selection OK
					regs[REG_INTS] |= INTS_CommandComplete;
					isBusy  = true;
					msgin   =  0;
					counter = -1;
					err     =  false;
					if (atn) {
						phase          = SCSI::MSG_OUT;
						nextPhase      = SCSI::COMMAND;
						regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGOUT;
					} else {
						phase          = SCSI::COMMAND;
						nextPhase      = SCSI::UNDEFINED;
						regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_COMMAND;
					}
				} else {
					err = true;
				}
			} else {
				err = true;
			}

			if (err) {
				// target selection error
				regs[REG_INTS] |= INTS_TimeOut;
				disconnect();
			}
			break;
		}
		// hardware transfer
		case CMD_Transfer:
			if ((regs[FIX_PCTL] == (regs[REG_PSNS] & 7)) &&
			    (regs[REG_PSNS] & (PSNS_REQ | PSNS_BSY))) {
				isTransfer = true; // set Xfer in Progress
			} else {
				// phase error
				regs[REG_INTS] |= INTS_ServiceRequited;
			}
			break;

		case CMD_BusRelease:
			disconnect();
			break;

		case CMD_SetATN:
			atn = PSNS_ATN;
			break;

		case CMD_ResetATN:
			atn = 0;
			break;

		case CMD_TransferPause:
			// nothing is done in the initiator.
			break;
		}
		break;  // end of REG_SCMD
	}
	case REG_INTS: // Reset Interrupts
		regs[REG_INTS] &= ~value;
		if (rst) {
			regs[REG_INTS] |= INTS_ResetCondition;
		}
		break;

	case REG_TEMP:
		regs[REG_TEMPWR] = value;
		break;

	case REG_TCL:
		tc = (tc & 0xFFFF00) + (value <<  0);
		break;

	case REG_TCM:
		tc = (tc & 0xFF00FF) + (value <<  8);
		break;

	case REG_TCH:
		tc = (tc & 0x00FFFF) + (value << 16);
		break;

	case REG_PCTL:
		regs[REG_PCTL] = value;
		regs[FIX_PCTL] = value & 7;
		break;

	case REG_BDID:
		// set Bus Device ID
		value &= 7;
		myId = value;
		regs[REG_BDID] = uint8_t(1 << value);
		break;

		// Nothing
	case REG_SDGC:
	case REG_SSTS:
	case REG_SERR:
	case REG_MBC:
	case 15:
		break;

	case REG_SCTL: {
		bool flag = !(value & 0xE0);
		if (flag != isEnabled) {
			isEnabled = flag;
			if (!flag) {
				softReset();
			}
		}
		[[fallthrough]];
	}
	default:
		regs[reg] = value;
	}
}

uint8_t MB89352::getSSTS() const
{
	uint8_t result = 1; // set fifo empty
	if (isTransfer) {
		if (regs[REG_PSNS] & PSNS_IO) { // SCSI -> SPC transfer
			if (tc >= 8) {
				result = 2; // set fifo full
			} else {
				if (tc != 0) {
					result = 0; // set fifo 1..7 bytes
				}
			}
		}
	}
	if (phase != SCSI::BUS_FREE) {
		result |= 0x80; // set indicator
	}
	if (isBusy) {
		result |= 0x20; // set SPC_BSY
	}
	if ((phase >= SCSI::COMMAND) || isTransfer) {
		result |= 0x10; // set Xfer in Progress
	}
	if (rst) {
		result |= 0x08; // set SCSI RST
	}
	if (tc == 0) {
		result |= 0x04; // set tc = 0
	}
	return result;
}

uint8_t MB89352::readRegister(uint8_t reg)
{
	switch (reg) {
	case REG_DREG:
		return readDREG();

	case REG_PSNS:
		if (phase == SCSI::EXECUTE) {
			counter = narrow<int>(dev[targetId]->executingCmd(phase, blockCounter));
			if (atn && phase != SCSI::EXECUTE) {
				nextPhase = phase;
				phase = SCSI::MSG_OUT;
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGOUT;
			} else {
				switch (phase) {
				case SCSI::DATA_IN:
					regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAIN;
					break;
				case SCSI::DATA_OUT:
					regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
					break;
				case SCSI::STATUS:
					regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_STATUS;
					break;
				case SCSI::EXECUTE:
					regs[REG_PSNS] = PSNS_BSY;
					break;
				default:
					// phase error
					break;
				}
			}
		}
		return regs[REG_PSNS] | atn;

	default:
		return peekRegister(reg);
	}
}

uint8_t MB89352::peekDREG() const
{
	if (isTransfer && (tc > 0)) {
		return regs[REG_DREG];
	} else {
		return 0xFF;
	}
}

uint8_t MB89352::peekRegister(uint8_t reg) const
{
	switch (reg) {
	case REG_DREG:
		return peekDREG();
	case REG_PSNS:
		return regs[REG_PSNS] | atn;
	case REG_SSTS:
		return getSSTS();
	case REG_TCH:
		return narrow_cast<uint8_t>((tc >> 16) & 0xFF);
	case REG_TCM:
		return narrow_cast<uint8_t>((tc >>  8) & 0xFF);
	case REG_TCL:
		return narrow_cast<uint8_t>((tc >>  0) & 0xFF);
	default:
		return regs[reg];
	}
}


// TODO duplicated in WD33C93.cc
static constexpr std::initializer_list<enum_string<SCSI::Phase>> phaseInfo = {
	{ "UNDEFINED",   SCSI::UNDEFINED   },
	{ "BUS_FREE",    SCSI::BUS_FREE    },
	{ "ARBITRATION", SCSI::ARBITRATION },
	{ "SELECTION",   SCSI::SELECTION   },
	{ "RESELECTION", SCSI::RESELECTION },
	{ "COMMAND",     SCSI::COMMAND     },
	{ "EXECUTE",     SCSI::EXECUTE     },
	{ "DATA_IN",     SCSI::DATA_IN     },
	{ "DATA_OUT",    SCSI::DATA_OUT    },
	{ "STATUS",      SCSI::STATUS      },
	{ "MSG_OUT",     SCSI::MSG_OUT     },
	{ "MSG_IN",      SCSI::MSG_IN      }
};
SERIALIZE_ENUM(SCSI::Phase, phaseInfo);

template<typename Archive>
void MB89352::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("buffer", buffer);
	std::array<char, 8> tag = {'d', 'e', 'v', 'i', 'c', 'e', 'X', 0};
	for (auto [i, d] : enumerate(dev)) {
		tag[6] = char('0' + i);
		ar.serializePolymorphic(tag.data(), *d);
	}
	ar.serialize("bufIdx",       bufIdx,
	             "msgin",        msgin,
	             "counter",      counter,
	             "blockCounter", blockCounter,
	             "tc",           tc,
	             "phase",        phase,
	             "nextPhase",    nextPhase,
	             "myId",         myId,
	             "targetId",     targetId);
	ar.serialize_blob("registers", regs);
	ar.serialize("rst",        rst,
	             "atn",        atn,
	             "isEnabled",  isEnabled,
	             "isBusy",     isBusy,
	             "isTransfer", isTransfer,
	             "cdbIdx",     cdbIdx);
	ar.serialize_blob("cdb", cdb);
}
INSTANTIATE_SERIALIZE_METHODS(MB89352);

} // namespace openmsx
