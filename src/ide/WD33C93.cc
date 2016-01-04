/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/wd33c93.c,v
** Revision: 1.12
** Date: 2007/03/25 17:05:07
**
** Based on the WD33C93 emulation in MESS (www.mess.org).
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik, Tomas Karlsson, white cat
*/

#include "WD33C93.hh"
#include "SCSI.hh"
#include "SCSIDevice.hh"
#include "DummySCSIDevice.hh"
#include "SCSIHD.hh"
#include "SCSILS120.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "memory.hh"
#include <cassert>
#include <cstring>

namespace openmsx {

static const unsigned MAX_DEV = 8;

static const byte REG_OWN_ID      = 0x00;
static const byte REG_CONTROL     = 0x01;
static const byte REG_TIMEO       = 0x02;
static const byte REG_TSECS       = 0x03;
static const byte REG_THEADS      = 0x04;
static const byte REG_TCYL_HI     = 0x05;
static const byte REG_TCYL_LO     = 0x06;
static const byte REG_ADDR_HI     = 0x07;
static const byte REG_ADDR_2      = 0x08;
static const byte REG_ADDR_3      = 0x09;
static const byte REG_ADDR_LO     = 0x0a;
static const byte REG_SECNO       = 0x0b;
static const byte REG_HEADNO      = 0x0c;
static const byte REG_CYLNO_HI    = 0x0d;
static const byte REG_CYLNO_LO    = 0x0e;
static const byte REG_TLUN        = 0x0f;
static const byte REG_CMD_PHASE   = 0x10;
static const byte REG_SYN         = 0x11;
static const byte REG_TCH         = 0x12;
static const byte REG_TCM         = 0x13;
static const byte REG_TCL         = 0x14;
static const byte REG_DST_ID      = 0x15;
static const byte REG_SRC_ID      = 0x16;
static const byte REG_SCSI_STATUS = 0x17; // (r)
static const byte REG_CMD         = 0x18;
static const byte REG_DATA        = 0x19;
static const byte REG_QUEUE_TAG   = 0x1a;
static const byte REG_AUX_STATUS  = 0x1f; // (r)

static const byte REG_CDBSIZE     = 0x00;
static const byte REG_CDB1        = 0x03;
static const byte REG_CDB2        = 0x04;
static const byte REG_CDB3        = 0x05;
static const byte REG_CDB4        = 0x06;
static const byte REG_CDB5        = 0x07;
static const byte REG_CDB6        = 0x08;
static const byte REG_CDB7        = 0x09;
static const byte REG_CDB8        = 0x0a;
static const byte REG_CDB9        = 0x0b;
static const byte REG_CDB10       = 0x0c;
static const byte REG_CDB11       = 0x0d;
static const byte REG_CDB12       = 0x0e;

static const byte OWN_EAF         = 0x08; // ENABLE ADVANCED FEATURES

// SCSI STATUS
static const byte SS_RESET        = 0x00; // reset
static const byte SS_RESET_ADV    = 0x01; // reset w/adv. features
static const byte SS_XFER_END     = 0x16; // select and transfer complete
static const byte SS_SEL_TIMEOUT  = 0x42; // selection timeout
static const byte SS_DISCONNECT   = 0x85;

// AUX STATUS
static const byte AS_DBR          = 0x01; // data buffer ready
static const byte AS_CIP          = 0x10; // command in progress, chip is busy
static const byte AS_BSY          = 0x20; // Level 2 command in progress
static const byte AS_LCI          = 0x40; // last command ignored
static const byte AS_INT          = 0x80;

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

WD33C93::WD33C93(const DeviceConfig& config)
{
	devBusy = false;

	for (auto* t : config.getXML()->getChildren("target")) {
		unsigned id = t->getAttributeAsInt("id");
		if (id >= MAX_DEV) {
			throw MSXException(StringOp::Builder() <<
				"Invalid SCSI id: " << id <<
				" (should be 0.." << MAX_DEV - 1 << ')');
		}
		if (dev[id]) {
			throw MSXException(StringOp::Builder() <<
				"Duplicate SCSI id: " << id);
		}
		DeviceConfig conf(config, *t);
		auto& type = t->getChild("type").getData();
		if (type == "SCSIHD") {
			dev[id] = make_unique<SCSIHD>(conf, buffer,
			        SCSIDevice::MODE_SCSI1 | SCSIDevice::MODE_UNITATTENTION |
			        SCSIDevice::MODE_NOVAXIS);
		} else if (type == "SCSILS120") {
			dev[id] = make_unique<SCSILS120>(conf, buffer,
			        SCSIDevice::MODE_SCSI1 | SCSIDevice::MODE_UNITATTENTION |
			        SCSIDevice::MODE_NOVAXIS);
		} else {
			throw MSXException("Unknown SCSI device: " + type);
		}
	}
	// fill remaining targets with dummy SCSI devices to prevent crashes
	for (auto& d : dev) {
		if (!d) d = make_unique<DummySCSIDevice>();
	}
	reset(false);

	// avoid UMR on savestate
	memset(buffer.data(), 0, SCSIDevice::BUFFER_SIZE);
	counter = 0;
	blockCounter = 0;
	targetId = 0;
}

WD33C93::~WD33C93()
{
}

void WD33C93::disconnect()
{
	if (phase != SCSI::BUS_FREE) {
		assert(targetId < MAX_DEV);
		dev[targetId]->disconnect();
		if (regs[REG_SCSI_STATUS] != SS_XFER_END) {
			regs[REG_SCSI_STATUS] = SS_DISCONNECT;
		}
		regs[REG_AUX_STATUS] = AS_INT;
		phase = SCSI::BUS_FREE;
	}
	tc = 0;
}

void WD33C93::execCmd(byte value)
{
	if (regs[REG_AUX_STATUS] & AS_CIP) {
		// CIP error
		return;
	}
	//regs[REG_AUX_STATUS] |= AS_CIP;
	regs[REG_CMD] = value;

	bool atn = false;
	switch (value) {
	case 0x00: // Reset controller (software reset)
		memset(regs + 1, 0, 0x1a);
		disconnect();
		latch = 0; // TODO: is this correct? Some doc says: reset to zero by masterreset-signal but not by reset command.
		regs[REG_SCSI_STATUS] =
			(regs[REG_OWN_ID] & OWN_EAF) ? SS_RESET_ADV : SS_RESET;
		break;

	case 0x02: // Assert ATN
		break;

	case 0x04: // Disconnect
		disconnect();
		break;

	case 0x06: // Select with ATN (Lv2)
		atn = true;
		// fall-through
	case 0x07: // Select Without ATN (Lv2)
		targetId = regs[REG_DST_ID] & 7;
		regs[REG_SCSI_STATUS] = SS_SEL_TIMEOUT;
		tc = 0;
		regs[REG_AUX_STATUS] = AS_INT;
		break;

	case 0x08: // Select with ATN and transfer (Lv2)
		atn = true;
		// fall-through
	case 0x09: // Select without ATN and Transfer (Lv2)
		targetId = regs[REG_DST_ID] & 7;

		if (!devBusy && targetId < MAX_DEV && /* targetId != myId  && */
		    dev[targetId]->isSelected()) {
			if (atn) {
				dev[targetId]->msgOut(regs[REG_TLUN] | 0x80);
			}
			devBusy = true;
			counter = dev[targetId]->executeCmd(
				&regs[REG_CDB1], phase, blockCounter);

			switch (phase) {
			case SCSI::STATUS:
				devBusy = false;
				regs[REG_TLUN] = dev[targetId]->getStatusCode();
				dev[targetId]->msgIn();
				regs[REG_SCSI_STATUS] = SS_XFER_END;
				disconnect();
				break;

			case SCSI::EXECUTE:
				regs[REG_AUX_STATUS] = AS_CIP | AS_BSY;
				bufIdx = 0;
				break;

			default:
				devBusy = false;
				regs[REG_AUX_STATUS] = AS_CIP | AS_BSY | AS_DBR;
				bufIdx = 0;
			}
			//regs[REG_SRC_ID] |= regs[REG_DST_ID] & 7;
		} else {
			// timeout
			tc = 0;
			regs[REG_SCSI_STATUS] = SS_SEL_TIMEOUT;
			regs[REG_AUX_STATUS]  = AS_INT;
		}
		break;

	case 0x18: // Translate Address (Lv2)
	default:
		// unsupport command
		break;
	}
}

void WD33C93::writeAdr(byte value)
{
	latch = value & 0x1f;
}

// Latch incremented by one each time a register is accessed,
// except for the address-, aux.status-, data- and command registers.
void WD33C93::writeCtrl(byte value)
{
	switch (latch) {
	case REG_OWN_ID:
		regs[REG_OWN_ID] = value;
		myId = value & 7;
		break;

	case REG_TCH:
		tc = (tc & 0x00ffff) + (value << 16);
		break;

	case REG_TCM:
		tc = (tc & 0xff00ff) + (value <<  8);
		break;

	case REG_TCL:
		tc = (tc & 0xffff00) + (value <<  0);
		break;

	case REG_CMD_PHASE:
		regs[REG_CMD_PHASE] = value;
		break;

	case REG_CMD:
		execCmd(value);
		return; // no latch-inc for address-, aux.status-, data- and command regs.

	case REG_DATA:
		regs[REG_DATA] = value;
		if (phase == SCSI::DATA_OUT) {
			buffer[bufIdx++] = value;
			--tc;
			if (--counter == 0) {
				counter = dev[targetId]->dataOut(blockCounter);
				if (counter) {
					bufIdx = 0;
					return;
				}
				regs[REG_TLUN] = dev[targetId]->getStatusCode();
				dev[targetId]->msgIn();
				regs[REG_SCSI_STATUS] = SS_XFER_END;
				disconnect();
			}
		}
		return; // no latch-inc for address-, aux.status-, data- and command regs.

	case REG_AUX_STATUS:
		return; // no latch-inc for address-, aux.status-, data- and command regs.

	default:
		if (latch <= REG_SRC_ID) {
			regs[latch] = value;
		}
		break;
	}
	latch = (latch + 1) & 0x1f;
}

byte WD33C93::readAuxStatus()
{
	byte rv = regs[REG_AUX_STATUS];

	if (phase == SCSI::EXECUTE) {
		counter = dev[targetId]->executingCmd(phase, blockCounter);

		switch (phase) {
		case SCSI::STATUS: // TODO how can this ever be the case?
			regs[REG_TLUN] = dev[targetId]->getStatusCode();
			dev[targetId]->msgIn();
			regs[REG_SCSI_STATUS] = SS_XFER_END;
			disconnect();
			break;

		case SCSI::EXECUTE:
			break;

		default:
			regs[REG_AUX_STATUS] |= AS_DBR;
		}
	}
	return rv;
}

// Latch incremented by one each time a register is accessed,
// except for the address-, aux.status-, data- and command registers.
byte WD33C93::readCtrl()
{
	byte rv;
	switch (latch) {
	case REG_SCSI_STATUS:
		rv = regs[REG_SCSI_STATUS];
		if (rv != SS_XFER_END) {
			regs[REG_AUX_STATUS] &= ~AS_INT;
		} else {
			regs[REG_SCSI_STATUS] = SS_DISCONNECT;
			regs[REG_AUX_STATUS]  = AS_INT;
		}
		break;

	case REG_CMD:
		return regs[REG_CMD]; // no latch-inc for address-, aux.status-, data- and command regs.

	case REG_DATA:
		if (phase == SCSI::DATA_IN) {
			rv = buffer[bufIdx++];
			regs[REG_DATA] = rv;
			--tc;
			if (--counter == 0) {
				if (blockCounter > 0) {
					counter = dev[targetId]->dataIn(blockCounter);
					if (counter) {
						bufIdx = 0;
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
		return rv; // no latch-inc for address-, aux.status-, data- and command regs.

	case REG_TCH:
		rv = (tc >> 16) & 0xff;
		break;

	case REG_TCM:
		rv = (tc >>  8) & 0xff;
		break;

	case REG_TCL:
		rv = (tc >>  0) & 0xff;
		break;

	case REG_AUX_STATUS:
		return readAuxStatus(); // no latch-inc for address-, aux.status-, data- and command regs.

	default:
		rv = regs[latch];
		break;
	}

	latch = (latch + 1) & 0x1f;
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
	// initialized register
	memset(regs, 0, 0x1b);
	memset(regs + 0x1b, 0xff, 4);
	regs[REG_AUX_STATUS] = AS_INT;
	myId  = 0;
	latch = 0;
	tc    = 0;
	phase = SCSI::BUS_FREE;
	bufIdx  = 0;
	if (scsireset) {
		for (auto& d : dev) {
			d->reset();
		}
	}
}


static std::initializer_list<enum_string<SCSI::Phase>> phaseInfo = {
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
void WD33C93::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("buffer", buffer.data(), buffer.size());
	char tag[8] = { 'd', 'e', 'v', 'i', 'c', 'e', 'X', 0 };
	for (unsigned i = 0; i < MAX_DEV; ++i) {
		tag[6] = char('0' + i);
		ar.serializePolymorphic(tag, *dev[i]);
	}
	ar.serialize("bufIdx", bufIdx);
	ar.serialize("counter", counter);
	ar.serialize("blockCounter", blockCounter);
	ar.serialize("tc", tc);
	ar.serialize("phase", phase);
	ar.serialize("myId", myId);
	ar.serialize("targetId", targetId);
	ar.serialize_blob("registers", regs, sizeof(regs));
	ar.serialize("latch", latch);
	ar.serialize("devBusy", devBusy);
}
INSTANTIATE_SERIALIZE_METHODS(WD33C93);

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
	return scsiDeviceCreate(id, diskId, buffer, nullptr, type, mode,
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
	        buffer, nullptr, type, mode, (CdromXferCompCb)wd33c93XferCb, wd33c93);
	return dev;
#endif
}
*/

} // namespace openmsx
