/*****************************************************************************
** $Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/wd33c93.c,v $
**
** $Revision: 1.12 $
**
** $Date$
**
** Based on the WD33C93 emulation in MESS (www.mess.org).
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik, Tomas Karlsson, white cat
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/
#include "WD33C93.hh"
#include "SCSI.hh"
#include "SCSIDevice.hh"
#include "XMLElement.hh"
// TODO: #include "ArchCdrom.h"
#include <cassert>
#include <iostream>
#undef PRT_DEBUG
#define  PRT_DEBUG(mes)                          \
        do {                                    \
                std::cout << mes << std::endl;  \
        } while (0)


#define REG_OWN_ID      0x00
#define REG_CONTROL     0x01
#define REG_TIMEO       0x02
#define REG_TSECS       0x03
#define REG_THEADS      0x04
#define REG_TCYL_HI     0x05
#define REG_TCYL_LO     0x06
#define REG_ADDR_HI     0x07
#define REG_ADDR_2      0x08
#define REG_ADDR_3      0x09
#define REG_ADDR_LO     0x0a
#define REG_SECNO       0x0b
#define REG_HEADNO      0x0c
#define REG_CYLNO_HI    0x0d
#define REG_CYLNO_LO    0x0e
#define REG_TLUN        0x0f
#define REG_CMD_PHASE   0x10
#define REG_SYN         0x11
#define REG_TCH         0x12
#define REG_TCM         0x13
#define REG_TCL         0x14
#define REG_DST_ID      0x15
#define REG_SRC_ID      0x16
#define REG_SCSI_STATUS 0x17    // (r)
#define REG_CMD         0x18
#define REG_DATA        0x19
#define REG_QUEUE_TAG   0x1a
#define REG_AUX_STATUS  0x1f    // (r)

#define REG_CDBSIZE     0x00
#define REG_CDB1        0x03
#define REG_CDB2        0x04
#define REG_CDB3        0x05
#define REG_CDB4        0x06
#define REG_CDB5        0x07
#define REG_CDB6        0x08
#define REG_CDB7        0x09
#define REG_CDB8        0x0a
#define REG_CDB9        0x0b
#define REG_CDB10       0x0c
#define REG_CDB11       0x0d
#define REG_CDB12       0x0e

#define OWN_EAF         0x08    // ENABLE ADVANCED FEATURES

// SCSI STATUS
#define SS_RESET        0x00    // reset
#define SS_RESET_ADV    0x01    // reset w/adv. features
#define SS_XFER_END     0x16    // select and transfer complete
#define SS_SEL_TIMEOUT  0x42    // selection timeout
#define SS_DISCONNECT   0x85

// AUX STATUS
#define AS_DBR          0x01    // data buffer ready
#define AS_CIP          0x10    // command in progress, chip is busy
#define AS_BSY          0x20    // Level 2 command in progress
#define AS_LCI          0x40    // last command ignored
#define AS_INT          0x80

/*
#define MCI_IO          0x01
#define MCI_CD          0x02
#define MCI_MSG         0x04
#define MCI_COMMAND     MCI_CD
#define MCI_DATAIN      MCI_IO
#define MCI_DATAOUT     0
#define MCI_STATUS      (MCI_CD  | MCI_IO)
#define MCI_MSGIN       (MCI_MSG | MCI_CD | MCI_IO)
#define MCI_MSGOUT      (MCI_MSG | MCI_CD)
*/

/* command phase
0x00    NO_SELECT
0x10    SELECTED
0x20    IDENTIFY_SENT
0x30    COMMAND_OUT
0x41    SAVE_DATA_RECEIVED
0x42    DISCONNECT_RECEIVED
0x43    LEGAL_DISCONNECT
0x44    RESELECTED
0x45    IDENTIFY_RECEIVED
0x46    DATA_TRANSFER_DONE
0x47    STATUS_STARTED
0x50    STATUS_RECEIVED
0x60    COMPLETE_RECEIVED
*/

namespace openmsx {

void WD33C93::disconnect()
{
	if (phase != BusFree) {
		if ((targetId >= 0) && (targetId < maxDev)) {
			dev[targetId]->disconnect();
		}
		if (regs[REG_SCSI_STATUS] != SS_XFER_END) {
			regs[REG_SCSI_STATUS] = SS_DISCONNECT;
		}
		regs[REG_AUX_STATUS]  = AS_INT;
		phase          = BusFree;
		//nextPhase    = -1;
	}

	//mci          = -1;
	tc             = 0;
	//atn          = 0;

	PRT_DEBUG("busfree()");
}

/*
static void WD33C93::irq(unsigned time)
{
    //timerRunning = 0;

    // set IRQ flag
    regs[REG_AUX_STATUS] = AS_DBR | AS_INT;
    // clear busy flags
    regs[REG_AUX_STATUS] &= ~(AS_CIP | AS_BSY);

    // select w/ATN and transfer
    regs[REG_SCSI_STATUS] = SS_XFER_END;
    regs[REG_CMD_PHASE] = 0x60; // command successfully completed

    //setInt();
}
*/

void WD33C93::xferCb(int length) // Manuel: length??
{
	devBusy = 0;
}

void WD33C93::execCmd(byte value)
{
	int atn = 0;

	if (regs[REG_AUX_STATUS] & AS_CIP) {
		PRT_DEBUG("wd33c93ExecCmd() CIP error");
		return;
	}
	//regs[REG_AUX_STATUS] |= AS_CIP;
	regs[REG_CMD] = value;
	switch (value) {
		case 0x00: /* Reset controller (software reset) */
			PRT_DEBUG("wd33c93 [CMD] Reset controller");
			memset(regs + 1, 0, 0x1a);
			disconnect();
			latch  = 0;
			regs[REG_SCSI_STATUS] =
				(regs[REG_OWN_ID] & OWN_EAF) ? SS_RESET_ADV : SS_RESET;
			break;

		case 0x02:  /* Assert ATN */
			PRT_DEBUG("wd33c93 [CMD] Assert ATN");
			break;

		case 0x04:  /* Disconnect */
			PRT_DEBUG("wd33c93 [CMD] Disconnect");
			disconnect();
			break;

		case 0x06:  /* Select with ATN (Lv2) */
			atn = 1;
		case 0x07:  /* Select Without ATN (Lv2) */
			PRT_DEBUG("wd33c93 [CMD] Select (ATN " << (int)atn << ")");
			targetId = regs[REG_DST_ID] & 7;
			regs[REG_SCSI_STATUS] = SS_SEL_TIMEOUT;
			tc = 0;
			regs[REG_AUX_STATUS] = AS_INT;
			break;

		case 0x08:  /* Select with ATN and transfer (Lv2) */
			atn = 1;
		case 0x09:  /* Select without ATN and Transfer (Lv2) */
			PRT_DEBUG("wd33c93 [CMD] Select and transfer (ATN " << (int)atn << ")");
			targetId = regs[REG_DST_ID] & 7;

			if (!devBusy && targetId < maxDev && /* targetId != myId  && */ 
				(dev[targetId].get()!=NULL) && // TODO: use dummy
					dev[targetId]->isSelected() ) {
				if (atn) dev[targetId]->msgOut(regs[REG_TLUN] | 0x80);
				devBusy = 1;
				counter = dev[targetId]->executeCmd(&regs[REG_CDB1],
						&phase, &blockCounter);

				switch (phase) {
					case Status:
						devBusy = 0;
						regs[REG_TLUN] = dev[targetId]->getStatusCode();
						dev[targetId]->msgIn();
						regs[REG_SCSI_STATUS] = SS_XFER_END;
						disconnect();
						break;

					case Execute:
						regs[REG_AUX_STATUS]  = AS_CIP | AS_BSY;
						pBuf = buffer;
						break;

					default:
						devBusy = 0;
						regs[REG_AUX_STATUS]  = AS_CIP | AS_BSY | AS_DBR;
						pBuf = buffer;
				}
				//regs[REG_SRC_ID] |= regs[REG_DST_ID] & 7;
			} else {
				//timeout = boardSystemTime() + boardFrequency() / 16384;
				//timerRunning = 1;
				//boardTimerAdd(timer, timeout);
				PRT_DEBUG("wd33c93 timeout on target " << (int)targetId);
				tc = 0;
				regs[REG_SCSI_STATUS]  = SS_SEL_TIMEOUT;
				regs[REG_AUX_STATUS]   = AS_INT;
			}
			break;

		case 0x18:  /* Translate Address (Lv2) */
		default:
			PRT_DEBUG("wd33c93 [CMD] unsupport command " << (int)value);
			break;
	}
}

void WD33C93::writeAdr(byte value)
{
	//PRT_DEBUG("WriteAdr value " << std::hex << (int)value);
	latch = value & 0x1f;
}

void WD33C93::writeCtrl(byte value)
{
	//PRT_DEBUG("wd33c93 write #" << std::hex << (int)latch << ", " << (int)value);
	switch (latch) {
		case REG_OWN_ID:
			regs[REG_OWN_ID] = value;
			myId = value & 7;
			PRT_DEBUG("wd33c93 myid = " << (int)myId);
			break;

		case REG_TCH:
			tc = (tc & 0x0000ffff) + (value << 16);
			break;

		case REG_TCM:
			tc = (tc & 0x00ff00ff) + (value << 8);
			break;

		case REG_TCL:
			tc = (tc & 0x00ffff00) + value;
			break;

		case REG_CMD_PHASE:
			PRT_DEBUG("wd33c93 CMD_PHASE = " << (int)value);
			regs[REG_CMD_PHASE] = value;
			break;

		case REG_CMD:
			execCmd(value);
			return;     

		case REG_DATA:
			regs[REG_DATA] = value;
			if (phase == DataOut) {
				*pBuf++ = value;
				--tc;
				if (--counter == 0) {
					counter = dev[targetId]->dataOut(&blockCounter);
					if (counter) {
						pBuf = buffer;
						return;
					}
					regs[REG_TLUN] = dev[targetId]->getStatusCode();
					dev[targetId]->msgIn();
					regs[REG_SCSI_STATUS] = SS_XFER_END;
					disconnect();
				}
			}
			return;

		case REG_AUX_STATUS:
			return;

		default:
			if (latch <= REG_SRC_ID) {
				regs[latch] = value;
			}
			break;
	}
	latch++;
	latch &= 0x1f;
}

byte WD33C93::readAuxStatus()
{
	byte rv = regs[REG_AUX_STATUS];

	if (phase == Execute) {
		counter =
			dev[targetId]->executingCmd(&phase, &blockCounter);

		switch (phase) {
			case Status:
				regs[REG_TLUN] = dev[targetId]->getStatusCode();
				dev[targetId]->msgIn();
				regs[REG_SCSI_STATUS] = SS_XFER_END;
				disconnect();
				break;

			case Execute:
				break;

			default:
				regs[REG_AUX_STATUS] |= AS_DBR;
		}
	}
	//PRT_DEBUG("readAuxStatus returning " << std::hex << (int)rv);
	return rv;
}

byte WD33C93::readCtrl()
{
	//PRT_DEBUG("ReadCtrl");
	byte rv;

	switch (latch) {
		case REG_SCSI_STATUS:
			rv = regs[REG_SCSI_STATUS];
			//PRT_DEBUG1("wd33c93 SCSI_STATUS = %X\n", rv);
			if (rv != SS_XFER_END) {
				regs[REG_AUX_STATUS] &= ~AS_INT;
			} else {
				regs[REG_SCSI_STATUS] = SS_DISCONNECT;
				regs[REG_AUX_STATUS]  = AS_INT;
			}
			break;

		case REG_DATA:
			if (phase == DataIn) {
				rv = *pBuf++;
				regs[REG_DATA] = rv;
				--tc;
				if (--counter == 0) {
					if (blockCounter > 0) {
						counter = dev[targetId]->dataIn(&blockCounter);
						if (counter) {
							pBuf = buffer;
							return rv;
						}
					}
					regs[REG_TLUN] = dev[targetId]->getStatusCode();
					dev[targetId]->msgIn();
					regs[REG_SCSI_STATUS] = SS_XFER_END;
					disconnect();
				}
			} else {
				rv = regs[REG_DATA];
			}
			return rv;

		case REG_TCH:
			rv = (byte)((tc >> 16) & 0xff);
			break;

		case REG_TCM:
			rv = (byte)((tc >> 8) & 0xff);
			break;

		case REG_TCL:
			rv = (byte)(tc & 0xff);
			break;

		case REG_AUX_STATUS:
			return readAuxStatus();

		default:
			rv = regs[latch];
			break;
	}
	//PRT_DEBUG2("wd33c93 read #%X, %X\n", latch, rv);

	if (latch != REG_CMD) {
		latch++;
		latch &= 0x1f;
	}
	return rv;
}

byte WD33C93::peekAuxStatus() const
{
	return regs[REG_AUX_STATUS];
}

byte WD33C93::peekCtrl() const
{
	switch (latch) {
	case REG_TCH:
		return (tc >> 16) & 0xff;
	case REG_TCM:
		return (tc >>  8) & 0xff;
	case REG_TCL:
		return (tc >>  0) & 0xff;
	default:
		return regs[latch];
	}
}

void WD33C93::reset(bool scsireset)
{
	int i;

	PRT_DEBUG("wd33c93 reset");

	// initialized register
	memset(regs, 0, 0x1b);
	memset(regs + 0x1b, 0xff, 4);
	regs[REG_AUX_STATUS] = AS_INT;
	myId   = 0;
	latch  = 0;
	tc     = 0;
	phase  = BusFree;
	pBuf   = buffer;
	if (scsireset) {
		for (i = 0; i < maxDev; ++i) {
			if ((dev[i].get()) != NULL) dev[i]->reset(); // TODO: use Dummy
		}
	}
}

WD33C93::~WD33C93()
{
	//boardTimerDestroy(timer);
	PRT_DEBUG("WD33C93 destroy");
	//TODO: archCdromBufferFree(buffer);
	free(buffer);
}

/* Here is some info on the parameters for SCSI devices:
static SCSIDevice* wd33c93ScsiDevCreate(WD33C93* wd33c93, int id)
{
#if 1
    // CD_UPDATE: Use dynamic parameters instead of hard coded ones
    int diskId, mode, type;

    diskId = diskGetHdDriveId(hdId, id);
    if (diskIsCdrom(diskId)) {
        mode = MODE_SCSI1 | MODE_UNITATTENTION | MODE_REMOVABLE | MODE_NOVAXIS;
        type = SDT_CDROM;
    } else {
        mode = MODE_SCSI1 | MODE_UNITATTENTION | MODE_FDS120 | MODE_REMOVABLE | MODE_NOVAXIS;
        type = SDT_DirectAccess;
    }
    return scsiDeviceCreate(id, diskId, buffer, NULL, type, mode,
                           (CdromXferCompCb)wd33c93XferCb, wd33c93);
#else
    SCSIDEVICE* dev;
    int mode;
    int type;

    if (id != 2) {
        mode = MODE_SCSI1 | MODE_UNITATTENTION | MODE_FDS120 | MODE_REMOVABLE | MODE_NOVAXIS;
        type = SDT_DirectAccess;
    } else {
        mode = MODE_SCSI1 | MODE_UNITATTENTION | MODE_REMOVABLE | MODE_NOVAXIS;
        type = SDT_CDROM;
    }
    dev = scsiDeviceCreate(id, diskGetHdDriveId(hdId, id),
            buffer, NULL, type, mode, (CdromXferCompCb)wd33c93XferCb, wd33c93);
    return dev;
#endif
}
*/
WD33C93::WD33C93(const XMLElement& config)
{
	// TODO: buffer  = archCdromBufferMalloc(BUFFER_SIZE);
	buffer = (byte*)calloc(1, BUFFER_SIZE);
	maxDev  = 8;
	devBusy = 0;
	//timer = boardTimerCreate(wd33c93Irq, wd33c93);

	XMLElement::Children targets;
	config.getChildren("target", targets);

	for (XMLElement::Children::const_iterator it = targets.begin(); 
	     it != targets.end(); ++it) {
		const XMLElement& target = **it;
		int id = target.getAttributeAsInt("id");
		assert (id < maxDev);
		const XMLElement& typeElem = target.getChild("type");
		const std::string& type = typeElem.getData();
		assert(type == "SCSIHD");
   
   		dev[id].reset(new SCSIDevice(id, buffer, NULL, SDT_DirectAccess, MODE_SCSI1 | MODE_UNITATTENTION | MODE_FDS120 | MODE_REMOVABLE | MODE_NOVAXIS));
	}
	reset(false);
}

} // namespace openmsx
