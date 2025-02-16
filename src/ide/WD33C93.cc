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

#include "DummySCSIDevice.hh"
#include "SCSI.hh"
#include "SCSIDevice.hh"
#include "SCSIHD.hh"
#include "SCSILS120.hh"

#include "DeviceConfig.hh"
#include "MSXException.hh"
#include "XMLElement.hh"
#include "serialize.hh"

#include "enumerate.hh"
#include "narrow.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>

namespace openmsx {

static constexpr unsigned MAX_DEV = 8;

static constexpr uint8_t REG_OWN_ID      = 0x00;
static constexpr uint8_t REG_CONTROL     = 0x01;
static constexpr uint8_t REG_TIMEO       = 0x02;
static constexpr uint8_t REG_TSECS       = 0x03;
static constexpr uint8_t REG_THEADS      = 0x04;
static constexpr uint8_t REG_TCYL_HI     = 0x05;
static constexpr uint8_t REG_TCYL_LO     = 0x06;
static constexpr uint8_t REG_ADDR_HI     = 0x07;
static constexpr uint8_t REG_ADDR_2      = 0x08;
static constexpr uint8_t REG_ADDR_3      = 0x09;
static constexpr uint8_t REG_ADDR_LO     = 0x0a;
static constexpr uint8_t REG_SECNO       = 0x0b;
static constexpr uint8_t REG_HEADNO      = 0x0c;
static constexpr uint8_t REG_CYLNO_HI    = 0x0d;
static constexpr uint8_t REG_CYLNO_LO    = 0x0e;
static constexpr uint8_t REG_TLUN        = 0x0f;
static constexpr uint8_t REG_CMD_PHASE   = 0x10;
static constexpr uint8_t REG_SYN         = 0x11;
static constexpr uint8_t REG_TCH         = 0x12;
static constexpr uint8_t REG_TCM         = 0x13;
static constexpr uint8_t REG_TCL         = 0x14;
static constexpr uint8_t REG_DST_ID      = 0x15;
static constexpr uint8_t REG_SRC_ID      = 0x16;
static constexpr uint8_t REG_SCSI_STATUS = 0x17; // (r)
static constexpr uint8_t REG_CMD         = 0x18;
static constexpr uint8_t REG_DATA        = 0x19;
static constexpr uint8_t REG_QUEUE_TAG   = 0x1a;
static constexpr uint8_t REG_AUX_STATUS  = 0x1f; // (r)

static constexpr uint8_t REG_CDBSIZE     = 0x00;
static constexpr uint8_t REG_CDB1        = 0x03;
static constexpr uint8_t REG_CDB2        = 0x04;
static constexpr uint8_t REG_CDB3        = 0x05;
static constexpr uint8_t REG_CDB4        = 0x06;
static constexpr uint8_t REG_CDB5        = 0x07;
static constexpr uint8_t REG_CDB6        = 0x08;
static constexpr uint8_t REG_CDB7        = 0x09;
static constexpr uint8_t REG_CDB8        = 0x0a;
static constexpr uint8_t REG_CDB9        = 0x0b;
static constexpr uint8_t REG_CDB10       = 0x0c;
static constexpr uint8_t REG_CDB11       = 0x0d;
static constexpr uint8_t REG_CDB12       = 0x0e;

static constexpr uint8_t OWN_EAF         = 0x08; // ENABLE ADVANCED FEATURES

// SCSI STATUS
static constexpr uint8_t SS_RESET        = 0x00; // reset
static constexpr uint8_t SS_RESET_ADV    = 0x01; // reset w/adv. features
static constexpr uint8_t SS_XFER_END     = 0x16; // select and transfer complete
static constexpr uint8_t SS_SEL_TIMEOUT  = 0x42; // selection timeout
static constexpr uint8_t SS_DISCONNECT   = 0x85;

// AUX STATUS
static constexpr uint8_t AS_DBR          = 0x01; // data buffer ready
static constexpr uint8_t AS_CIP          = 0x10; // command in progress, chip is busy
static constexpr uint8_t AS_BSY          = 0x20; // Level 2 command in progress
static constexpr uint8_t AS_LCI          = 0x40; // last command ignored
static constexpr uint8_t AS_INT          = 0x80;

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

WD33C93::WD33C93(DeviceConfig& config)
{
	for (auto* t : config.getXML()->getChildren("target")) {
		unsigned id = t->getAttributeValueAsInt("id", 0);
		if (id >= MAX_DEV) {
			throw MSXException("Invalid SCSI id: ", id,
			                   " (should be 0..", MAX_DEV - 1, ')');
		}
		if (dev[id]) {
			throw MSXException("Duplicate SCSI id: ", id);
		}
		DeviceConfig conf(config, *t);
		auto type = t->getChildData("type");
		if (type == "SCSIHD") {
			dev[id] = std::make_unique<SCSIHD>(conf, buffer,
			        SCSIDevice::MODE_SCSI1 | SCSIDevice::MODE_UNITATTENTION |
			        SCSIDevice::MODE_NOVAXIS);
		} else if (type == "SCSILS120") {
			dev[id] = std::make_unique<SCSILS120>(conf, buffer,
			        SCSIDevice::MODE_SCSI1 | SCSIDevice::MODE_UNITATTENTION |
			        SCSIDevice::MODE_NOVAXIS);
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
	std::ranges::fill(buffer, 0);
}

void WD33C93::disconnect()
{
	if (phase != SCSI::Phase::BUS_FREE) {
		assert(targetId < MAX_DEV);
		dev[targetId]->disconnect();
		if (regs[REG_SCSI_STATUS] != SS_XFER_END) {
			regs[REG_SCSI_STATUS] = SS_DISCONNECT;
		}
		regs[REG_AUX_STATUS] = AS_INT;
		phase = SCSI::Phase::BUS_FREE;
	}
	tc = 0;
}

void WD33C93::execCmd(uint8_t value)
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
		std::ranges::fill(subspan<0x1a>(regs, 1), 0);
		disconnect();
		latch = 0; // TODO: is this correct? Some doc says: reset to zero by master-reset-signal but not by reset command.
		regs[REG_SCSI_STATUS] =
			(regs[REG_OWN_ID] & OWN_EAF) ? SS_RESET_ADV : SS_RESET;
		break;

	case 0x02: // Assert ATN
		break;

	case 0x04: // Disconnect
		disconnect();
		break;

	case 0x06: // Select with ATN (Lv2)
		//atn = true; // never read
		[[fallthrough]];
	case 0x07: // Select Without ATN (Lv2)
		targetId = regs[REG_DST_ID] & 7;
		regs[REG_SCSI_STATUS] = SS_SEL_TIMEOUT;
		tc = 0;
		regs[REG_AUX_STATUS] = AS_INT;
		break;

	case 0x08: // Select with ATN and transfer (Lv2)
		atn = true;
		[[fallthrough]];
	case 0x09: // Select without ATN and Transfer (Lv2)
		targetId = regs[REG_DST_ID] & 7;

		if (!devBusy && targetId < MAX_DEV && /* targetId != myId  && */
		    dev[targetId]->isSelected()) {
			if (atn) {
				dev[targetId]->msgOut(regs[REG_TLUN] | 0x80);
			}
			devBusy = true;
			counter = dev[targetId]->executeCmd(
				subspan<12>(regs, REG_CDB1), phase, blockCounter);

			switch (phase) {
			case SCSI::Phase::STATUS:
				devBusy = false;
				regs[REG_TLUN] = dev[targetId]->getStatusCode();
				dev[targetId]->msgIn();
				regs[REG_SCSI_STATUS] = SS_XFER_END;
				disconnect();
				break;

			case SCSI::Phase::EXECUTE:
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
		// unsupported command
		break;
	}
}

void WD33C93::writeAdr(uint8_t value)
{
	latch = value & 0x1f;
}

// Latch incremented by one each time a register is accessed,
// except for the address-, aux.status-, data- and command registers.
void WD33C93::writeCtrl(uint8_t value)
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
		if (phase == SCSI::Phase::DATA_OUT) {
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

uint8_t WD33C93::readAuxStatus()
{
	uint8_t rv = regs[REG_AUX_STATUS];

	if (phase == SCSI::Phase::EXECUTE) {
		counter = dev[targetId]->executingCmd(phase, blockCounter);

		switch (phase) {
		case SCSI::Phase::STATUS: // TODO how can this ever be the case?
			regs[REG_TLUN] = dev[targetId]->getStatusCode();
			dev[targetId]->msgIn();
			regs[REG_SCSI_STATUS] = SS_XFER_END;
			disconnect();
			break;

		case SCSI::Phase::EXECUTE:
			break;

		default:
			regs[REG_AUX_STATUS] |= AS_DBR;
		}
	}
	return rv;
}

// Latch incremented by one each time a register is accessed,
// except for the address-, aux.status-, data- and command registers.
uint8_t WD33C93::readCtrl()
{
	uint8_t rv;
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
		if (phase == SCSI::Phase::DATA_IN) {
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
		rv = narrow_cast<uint8_t>((tc >> 16) & 0xff);
		break;

	case REG_TCM:
		rv = narrow_cast<uint8_t>((tc >>  8) & 0xff);
		break;

	case REG_TCL:
		rv = narrow_cast<uint8_t>((tc >>  0) & 0xff);
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

uint8_t WD33C93::peekAuxStatus() const
{
	return regs[REG_AUX_STATUS];
}

uint8_t WD33C93::peekCtrl() const
{
	switch (latch) {
	case REG_TCH:
		return narrow_cast<uint8_t>((tc >> 16) & 0xff);
	case REG_TCM:
		return narrow_cast<uint8_t>((tc >>  8) & 0xff);
	case REG_TCL:
		return narrow_cast<uint8_t>((tc >>  0) & 0xff);
	default:
		return regs[latch];
	}
}

void WD33C93::reset(bool scsiReset)
{
	// initialized register
	std::ranges::fill(subspan<0x1b>(regs, 0x00), 0x00);
	std::ranges::fill(subspan<0x04>(regs, 0x1b), 0xff);
	regs[REG_AUX_STATUS] = AS_INT;
	myId  = 0;
	latch = 0;
	tc    = 0;
	phase = SCSI::Phase::BUS_FREE;
	bufIdx  = 0;
	if (scsiReset) {
		for (const auto& d : dev) {
			d->reset();
		}
	}
}


static constexpr std::initializer_list<enum_string<SCSI::Phase>> phaseInfo = {
	{ "UNDEFINED",   SCSI::Phase::UNDEFINED   },
	{ "BUS_FREE",    SCSI::Phase::BUS_FREE    },
	{ "ARBITRATION", SCSI::Phase::ARBITRATION },
	{ "SELECTION",   SCSI::Phase::SELECTION   },
	{ "RESELECTION", SCSI::Phase::RESELECTION },
	{ "COMMAND",     SCSI::Phase::COMMAND     },
	{ "EXECUTE",     SCSI::Phase::EXECUTE     },
	{ "DATA_IN",     SCSI::Phase::DATA_IN     },
	{ "DATA_OUT",    SCSI::Phase::DATA_OUT    },
	{ "STATUS",      SCSI::Phase::STATUS      },
	{ "MSG_OUT",     SCSI::Phase::MSG_OUT     },
	{ "MSG_IN",      SCSI::Phase::MSG_IN      }
};
SERIALIZE_ENUM(SCSI::Phase, phaseInfo);

template<typename Archive>
void WD33C93::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("buffer", buffer);
	std::array<char, 8> tag = {'d', 'e', 'v', 'i', 'c', 'e', 'X', 0};
	for (auto [i, d] : enumerate(dev)) {
		tag[6] = char('0' + i);
		ar.serializePolymorphic(tag.data(), *d);
	}
	ar.serialize("bufIdx",       bufIdx,
	             "counter",      counter,
	             "blockCounter", blockCounter,
	             "tc",           tc,
	             "phase",        phase,
	             "myId",         myId,
	             "targetId",     targetId);
	ar.serialize_blob("registers", regs);
	ar.serialize("latch",   latch,
	             "devBusy", devBusy);
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
