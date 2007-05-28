// $Id$
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
 *  Not suppport padding transfer and interrupt signal. (Not used MEGA-SCSI)
 *  Message system might be imperfect. (Not used in MEGA-SCSI usually)
 */
#include "MB89352.hh"
#include "SCSIDevice.hh"
#include "DummySCSIDevice.hh"
#include "SCSI.hh"
#include "SCSIHD.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "StringOp.hh"
#include <cassert>
#include <string>
#include <iostream>
// TEMPORARY, for DEBUGGING:
#undef PRT_DEBUG
#define  PRT_DEBUG(mes)                          \
        do {                                    \
                std::cout << mes << std::endl;  \
        } while (0)

#define REG_BDID  0     // Bus Device ID        (r/w)
#define REG_SCTL  1     // Spc Control          (r/w)
#define REG_SCMD  2     // Command              (r/w)
#define REG_OPEN  3     //                      (open)
#define REG_INTS  4     // Interrupt Sense      (r/w)
#define REG_PSNS  5     // Phase Sense          (r)
#define REG_SDGC  5     // SPC Diag. Control    (w)
#define REG_SSTS  6     // SPC SCSI::STATUS           (r)
#define REG_SERR  7     // SPC Error SCSI::STATUS     (r/w?)
#define REG_PCTL  8     // Phase Control        (r/w)
#define REG_MBC   9     // Modified Byte Counter(r)
#define REG_DREG 10     // Data Register        (r/w)
#define REG_TEMP 11     // Temporary Register   (r/w)
                        //   Another value is maintained respec-
                        //   tively for writing and for reading

#define REG_TCH  12     // Transfer Counter High(r/w)
#define REG_TCM  13     // Transfer Counter Mid (r/w)
#define REG_TCL  14     // Transfer Counter Low (r/w)

#define REG_TEMPWR 13   // (TEMP register preservation place for writing)
#define FIX_PCTL   14   // (REG_PCTL & 7)

#define PSNS_IO     0x01
#define PSNS_CD     0x02
#define PSNS_MSG    0x04
#define PSNS_BSY    0x08
#define PSNS_SEL    0x10
#define PSNS_ATN    0x20
#define PSNS_ACK    0x40
#define PSNS_REQ    0x80

#define PSNS_SELECTION  PSNS_SEL
#define PSNS_COMMAND    PSNS_CD
#define PSNS_DATAIN     PSNS_IO
#define PSNS_DATAOUT    0
#define PSNS_STATUS     (PSNS_CD  | PSNS_IO)
#define PSNS_MSGIN      (PSNS_MSG | PSNS_CD | PSNS_IO)
#define PSNS_MSGOUT     (PSNS_MSG | PSNS_CD)

#define INTS_ResetCondition     0x01
#define INTS_SPC_HardError      0x02
#define INTS_TimeOut            0x04
#define INTS_ServiceRequited    0x08
#define INTS_CommandComplete    0x10
#define INTS_Disconnected       0x20
#define INTS_ReSelected         0x40
#define INTS_Selected           0x80

#define CMD_BusRelease      0
#define CMD_Select          1
#define CMD_ResetATN        2
#define CMD_SetATN          3
#define CMD_Transfer        4
#define CMD_TransferPause   5
#define CMD_Reset_ACK_REQ   6
#define CMD_Set_ACK_REQ     7

using std::string;

namespace openmsx {

static const unsigned MAX_DEV = 8;

MB89352::MB89352(MSXMotherBoard& motherBoard, const XMLElement& config)
	: buffer(SCSIDevice::BUFFER_SIZE)
{
	PRT_DEBUG("spc create");
	//TODO: devBusy = false;

	// ALMOST COPY PASTED FROM WD33C93:
	
	XMLElement::Children targets;
	config.getChildren("target", targets);
	for (XMLElement::Children::const_iterator it = targets.begin();
	     it != targets.end(); ++it) {
		const XMLElement& target = **it;
		unsigned id = target.getAttributeAsInt("id");
		if (id >= MAX_DEV) {
			throw MSXException(
				"Invalid SCSI id: " + StringOp::toString(id) +
				" (should be 0.." + StringOp::toString(MAX_DEV - 1) + ")");
		}
		if (dev[id].get()) {
			throw MSXException("Duplicate SCSI id: " + StringOp::toString(id));
		}
		const XMLElement& typeElem = target.getChild("type");
		const string& type = typeElem.getData();
		if (type == "SCSIHD") {
			dev[id].reset(new SCSIHD(motherBoard, target, &buffer[0], NULL,
			        SCSI::DT_DirectAccess,
			        SCSIDevice::MODE_SCSI2 | SCSIDevice::MODE_MEGASCSI |
			        SCSIDevice::MODE_FDS120 | SCSIDevice::MODE_REMOVABLE));
		} else {
			throw MSXException("Unknown SCSI device: " + type);
		}
	}
	// fill remaining targets with dummy SCSI devices to prevent crashes
	for (unsigned i = 0; i < MAX_DEV; ++i) {
		if (dev[i].get() == NULL) {
			dev[i].reset(new DummySCSIDevice());
		}
	}
	reset(0);
}

MB89352::~MB89352()
{
    PRT_DEBUG("spc destroy");
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
	isTransfer     = 0;
	counter        = 0;
	tc             = 0;
	atn            = 0;
}

void MB89352::softReset()
{
	isEnabled = 0;

	for (int i = 2; i < 15; ++i) {
		regs[i] = 0;
	}
	regs[15] = 0xFF;               // un mapped
	memset(cdb, 0, 12);

	pCdb   = cdb;
	bufIdx = 0;
	phase  = SCSI::BUS_FREE;
	disconnect();
}

void MB89352::reset(bool scsireset)
{
	//PRT_DEBUG("MB89352 reset");
	regs[REG_BDID] = 0x80;     // Initial value
	regs[REG_SCTL] = 0x80;
	rst  = false;
	atn  = 0;
	myId = 7;

	softReset();

	if (scsireset) {
		for (byte i = 0; i < MAX_DEV; ++i) {
			dev[i]->reset();
		}
	}
}

void MB89352::setACKREQ(byte* value)
{
	// REQ check
	if ((regs[REG_PSNS] & (PSNS_REQ | PSNS_BSY)) != (PSNS_REQ | PSNS_BSY)) {
		PRT_DEBUG("set ACK/REQ: REQ/BSY check error");
		if (regs[REG_PSNS] & PSNS_IO) { // SCSI -> SPC
			*value = 0xFF;
		}
		return;
	}

	// phase check
	if (regs[FIX_PCTL] != (regs[REG_PSNS] & 7)) {
		PRT_DEBUG("set ACK/REQ: phase check error");
		if (regs[REG_PSNS] & PSNS_IO) { // SCSI -> SPC
			*value = 0xFF;
		}
		if (isTransfer) {
			regs[REG_INTS] |= INTS_ServiceRequited;
		}
		return;
	}

	switch (phase) {

		// Transfer phase (data in)
		case SCSI::DATA_IN:
			*value = buffer[bufIdx];
			++bufIdx;
			regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_DATAIN;
			break;

		//Transfer phase (data out)
		case SCSI::DATA_OUT:
			buffer[bufIdx] = *value;
			++bufIdx;
			regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_DATAOUT;
			break;

		// Command phase
		case SCSI::COMMAND:
			if (counter < 0) {
				//Initialize command routine
				pCdb    = cdb;
				counter =  (*value < 0x20) ?  6 : (*value < 0xa0) ? 10 : 12;
			}
			*pCdb = *value;
			++pCdb;
			regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_COMMAND;
			break;

			// SCSI::STATUS phase
		case SCSI::STATUS:
			*value = dev[targetId]->getStatusCode();
			regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_STATUS;
			break;

			// Message In phase
		case SCSI::MSG_IN:
			*value = dev[targetId]->msgIn();
			regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_MSGIN;
			break;

			// Message Out phase
		case SCSI::MSG_OUT:
			msgin |= dev[targetId]->msgOut(*value);
			regs[REG_PSNS] = PSNS_ACK | PSNS_BSY | PSNS_MSGOUT;
			break;

		default:
			PRT_DEBUG("set ACK/REQ code error");
			break;
	}
} // end of setACKREQ()

void MB89352::resetACKREQ()
{
	// ACK check
	if ((regs[REG_PSNS] & (PSNS_ACK | PSNS_BSY)) != (PSNS_ACK | PSNS_BSY)) {
		PRT_DEBUG("reset ACK/REQ: ACK/BSY check error");
		return;
	}

	// phase check
	if (regs[FIX_PCTL] != (regs[REG_PSNS] & 7)) {
		PRT_DEBUG("reset ACK/REQ: phase check error");
		if (isTransfer) {
			regs[REG_INTS] |= INTS_ServiceRequited;
		}
		return;
	}

	switch (phase) {
		// Transfer phase (data in)
		case SCSI::DATA_IN:
			if (--counter > 0) {
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAIN;
			} else {
				if (blockCounter > 0) {
					counter = dev[targetId]->dataIn(blockCounter);
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

		// Transfer phase (data out)
		case SCSI::DATA_OUT:
			if (--counter > 0) {
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
			} else {
				counter = dev[targetId]->dataOut(blockCounter);
				if (counter) {
					regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_DATAOUT;
					bufIdx = 0;
					break;
				}
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_STATUS;
				phase = SCSI::STATUS;
			}
			break;

		// Command phase
		case SCSI::COMMAND:
			if (--counter > 0) {
				regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_COMMAND;
			} else {
				bufIdx = 0; // reset buffer index 
				//TODO: devBusy = true;
				counter =
					dev[targetId]->executeCmd(cdb, phase, blockCounter);

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
						return;
					default:
						PRT_DEBUG("phase error");
						break;
				}
				//TODO: devBusy = false;
			}
			break;

		// SCSI::STATUS phase
		case SCSI::STATUS:
			regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGIN;
			phase = SCSI::MSG_IN;
			break;

		// Message In phase
		case SCSI::MSG_IN:
			if (msgin <= 0) {
				disconnect();
				break;
			}
			msgin = 0;
			// throw to SCSI::MSG_OUT...

			// Message Out phase
		case SCSI::MSG_OUT:
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
					PRT_DEBUG("MsgOut code error");
					break;
			}
			return;

		default:
			//assert(false);
			PRT_DEBUG("reset ACK/REQ code error");
			break;
	}

	if (atn) {
		nextPhase = phase;
		phase = SCSI::MSG_OUT;
		regs[REG_PSNS] = PSNS_REQ | PSNS_BSY | PSNS_MSGOUT;
	}
} // end of mb89352ResetACKREQ()

byte MB89352::readDREG()
{
	if (isTransfer && (tc > 0)) {
		setACKREQ((byte*)&regs[REG_DREG]);
		resetACKREQ();
		//PRT_DEBUG("DREG read: " << tc << " " << std::hex << (int)regs[REG_DREG]);

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

void MB89352::writeDREG(byte value)
{
	if (isTransfer && (tc > 0)) {
		//PRT_DEBUG("DREG write: " << tc << " " << std::hex << (int)value);

		setACKREQ(&value);
		resetACKREQ();

		--tc;
		if (tc == 0) {
			isTransfer = false;
			regs[REG_INTS] |= INTS_CommandComplete;
		}
		regs[REG_MBC] = (regs[REG_MBC] - 1) & 0x0F;
	}
}

void MB89352::writeRegister(byte reg, byte value)
{
	//PRT_DEBUG("SPC write register: " << std::hex << (int)reg << " " << std::hex << (int)value);

	switch (reg) {

		case REG_DREG:
			// write data Register
			writeDREG(value);
			break;

		case REG_SCMD:
		{	// for local vars
			if (!isEnabled) {
				break;
			}

			// bus reset
			if (value & 0x10) {
				if (((regs[REG_SCMD] & 0x10) == 0) & (regs[REG_SCTL] == 0)) {
					PRT_DEBUG("SPC: bus reset");
					rst = true;
					regs[REG_INTS] |= INTS_ResetCondition;
					for (byte i = 0; i < MAX_DEV; ++i) {
						dev[i]->busReset();
					}
					disconnect();  // alternative routine
				}
			} else {
				rst = false;
			}

			regs[REG_SCMD] = value;

			int cmd = value >> 5;
			//PRT_DEBUG1("SPC command: %x\n", cmd);

			// execute spc command
			switch (cmd) {
				case CMD_Set_ACK_REQ:
					switch (phase) {
						case SCSI::DATA_IN:
						case SCSI::STATUS:
						case SCSI::MSG_IN:
							setACKREQ((byte*)&regs[REG_TEMP]);
							break;
						default:
							setACKREQ((byte*)&regs[REG_TEMPWR]);
					}
					break;

				case CMD_Reset_ACK_REQ:
					resetACKREQ();
					break;

				case CMD_Select:
				{ // for local vars
					if (rst) {
						regs[REG_INTS] |= INTS_TimeOut;
						break;
					}

					if (regs[REG_PCTL] & 1) {
						PRT_DEBUG("reselection error " << std::hex << (int)regs[REG_TEMPWR]);
						regs[REG_INTS] |= INTS_TimeOut;
						disconnect();
						break;
					}
					bool err = false;	
					int x = regs[REG_BDID] & regs[REG_TEMPWR];
					if (phase == SCSI::BUS_FREE && x && x != regs[REG_TEMPWR]) {
						x = regs[REG_TEMPWR] & ~regs[REG_BDID];

						// the targetID is calculated.
						// It is given priority that the number is large.
						for (targetId = 0; targetId < MAX_DEV; ++targetId) {
							x = x >> 1;
							if (x == 0) {
								break;
							}
						}

						if (/*!TODO: devBusy &&*/ dev[targetId]->isSelected()) {
							PRT_DEBUG("selection OK of target " << (int)targetId);
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
						PRT_DEBUG("selection error on target " << (int)targetId);
						regs[REG_INTS] |= INTS_TimeOut;
						disconnect();
					}
					break;
				}
				// hardware transfer
				case CMD_Transfer:
					PRT_DEBUG("CMD_Transfer " << tc << " ( " << (tc/512) << ")");
					if ((regs[FIX_PCTL] == (regs[REG_PSNS] & 7)) &&
							(regs[REG_PSNS] & (PSNS_REQ | PSNS_BSY))) {
						isTransfer = true;            // set Xfer in Progress
					} else {
						regs[REG_INTS] |= INTS_ServiceRequited;
						PRT_DEBUG("phase error");
					}
					break;

				case CMD_BusRelease:
					PRT_DEBUG("CMD_BusRelease");
					disconnect();
					break;

				case CMD_SetATN:
					PRT_DEBUG("CMD_SetATN");
					atn = PSNS_ATN;
					break;

				case CMD_ResetATN:
					PRT_DEBUG("CMD_ResetATN");
					atn = 0;
					break;

				case CMD_TransferPause:
					// nothing is done in the initiator.
					PRT_DEBUG("CMD_TransferPause");
					break;
			}
			break;  // end of REG_SCMD
		}
		// Reset Interrupts
		case REG_INTS:
			regs[REG_INTS] &= ~value;
			if (rst) regs[REG_INTS] |= INTS_ResetCondition;
			//PRT_DEBUG2("INTS reset: %x %x\n", value, regs[REG_INTS]);
			break;

		case REG_TEMP:
			regs[REG_TEMPWR] = value;
			break;

		case REG_TCL:
			tc = (tc & 0x00ffff00) + value;
			//PRT_DEBUG1("set tcl: %d\n", tc);
			break;

		case REG_TCM:
			tc = (tc & 0x00ff00ff) + (value << 8);
			//PRT_DEBUG1("set tcm: %d\n", tc);
			break;

		case REG_TCH:
			tc = (tc & 0x0000ffff) + (value << 16);
			//PRT_DEBUG1("set tch: %d\n", tc);
			break;

		case REG_PCTL:
			regs[REG_PCTL] = value;
			regs[FIX_PCTL] = value & 7;
			break;

		case REG_BDID:
			// set Bus Device ID
			value &= 7;
			myId = value;
			regs[REG_BDID] = (1 << value);
			break;

			// Nothing
		case REG_SDGC:
		case REG_SSTS:
		case REG_SERR:
		case REG_MBC:
		case 15:
			break;

		case REG_SCTL:
		{
			bool flag = value & 0xe0;
			if (flag != isEnabled) {
				isEnabled = flag;
				if (!flag) {
					softReset();
				}
			}
			// throw to default
		}
		default:
			regs[reg] = value;
	}
}  // end of writeRegister()

byte MB89352::getSSTS()
{
	byte result = 1;               // set fifo empty
	if (isTransfer) {
		if (regs[REG_PSNS] & PSNS_IO) { // SCSI -> SPC transfer
			if (tc >= 8) {
				result = 2;         // set fifo full
			} else {
				if (tc != 0) {
					result = 0;     // set fifo 1..7 bytes
				}
			}
		}
	}

	if (phase != SCSI::BUS_FREE) {
		result |= 0x80;             // set iniciator
	}

	if (isBusy) {
		result |= 0x20;             // set SPC_BSY
	}

	if ((phase >= SCSI::COMMAND) || isTransfer) {
		result |= 0x10;             // set Xfer in Progress
	}

	if (rst) {
		result |= 0x08;             // set SCSI RST
	}

	if (tc == 0) {
		result |= 0x04;             // set tc = 0
	}
	return result;
}

byte MB89352::readRegister(byte reg)
{
	//PRT_DEBUG("MB89352: Read register " << (int)reg);
	byte result;

	switch (reg) {
		case REG_DREG:
			result = readDREG();
			break;

		case REG_PSNS:
			if (phase == SCSI::EXECUTE) {
				counter =
					dev[targetId]->executingCmd(phase, blockCounter);

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
							PRT_DEBUG("phase error");
							break;
					}
				}
			}

			result = (byte)((regs[REG_PSNS] | atn) & 0xFF);
			break;

		case REG_SSTS:
			result = getSSTS();
			break;

		case REG_TCL:
			result = (byte)(tc & 0xFF);
			break;

		case REG_TCM:
			result = (byte)((tc >>  8) & 0xFF);
			break;

		case REG_TCH:
			result = (byte)((tc >> 16) & 0xFF);
			break;

		default:
			result = (byte)(regs[reg] & 0xFF);
	}

	//PRT_DEBUG2("SPC reg read: %x %x\n", reg, result);

	return result;

} // end of readRegister()

byte MB89352::peekRegister(byte reg)
{
	switch (reg) {
		case REG_DREG:
			if (isTransfer && (tc > 0)) {
				return (byte)(regs[REG_DREG] & 0xFF);
			} else {
				return 0xFF;
			}

		case REG_PSNS:
			return (byte)((regs[REG_PSNS] | atn) & 0xFF);

		case REG_SSTS:
			return getSSTS();

		case REG_TCH:
			return (byte)((tc >> 16) & 0xFF);

		case REG_TCM:
			return (byte)((tc >>  8) & 0xFF);

		case REG_TCL:
			return (byte)(tc & 0xFF);

		default:
			return (byte)(regs[reg] & 0xFF);
	}
}

} // namespace openmsx

