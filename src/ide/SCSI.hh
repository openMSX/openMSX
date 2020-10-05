/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDefs.h,v
** Revision: 1.2
** Date: 2007/03/24 08:01:48
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/
#ifndef SCSI_HH
#define SCSI_HH

#include "openmsx.hh"

namespace openmsx::SCSI {

// Group 0: 6bytes cdb
constexpr byte OP_TEST_UNIT_READY       = 0x00;
constexpr byte OP_REZERO_UNIT           = 0x01;
constexpr byte OP_REQUEST_SENSE         = 0x03;
constexpr byte OP_FORMAT_UNIT           = 0x04;
constexpr byte OP_REASSIGN_BLOCKS       = 0x07;
constexpr byte OP_READ6                 = 0x08;
constexpr byte OP_WRITE6                = 0x0A;
constexpr byte OP_SEEK6                 = 0x0B;
constexpr byte OP_INQUIRY               = 0x12;
constexpr byte OP_RESERVE_UNIT          = 0x16;
constexpr byte OP_RELEASE_UNIT          = 0x17;
constexpr byte OP_MODE_SENSE            = 0x1A;
constexpr byte OP_START_STOP_UNIT       = 0x1B;
constexpr byte OP_SEND_DIAGNOSTIC       = 0x1D;

// Group 1: 10bytes cdb
constexpr byte OP_GROUP1                = 0x20;
constexpr byte OP_READ_CAPACITY         = 0x25;
constexpr byte OP_READ10                = 0x28;
constexpr byte OP_WRITE10               = 0x2A;
constexpr byte OP_SEEK10                = 0x2B;

constexpr byte OP_GROUP2                = 0x40;
constexpr byte OP_CHANGE_DEFINITION     = 0x40;
constexpr byte OP_READ_SUB_CHANNEL      = 0x42;
constexpr byte OP_READ_TOC              = 0x43;
constexpr byte OP_READ_HEADER           = 0x44;
constexpr byte OP_PLAY_AUDIO            = 0x45;
constexpr byte OP_PLAY_AUDIO_MSF        = 0x47;
constexpr byte OP_PLAY_TRACK_INDEX      = 0x48;
constexpr byte OP_PLAY_TRACK_RELATIVE   = 0x49;
constexpr byte OP_PAUSE_RESUME          = 0x4B;

constexpr byte OP_PLAY_AUDIO12          = 0xA5;
constexpr byte OP_READ12                = 0xA8;
constexpr byte OP_PLAY_TRACK_RELATIVE12 = 0xA9;
constexpr byte OP_READ_CD_MSF           = 0xB9;
constexpr byte OP_READ_CD               = 0xBE;

// Sense data                               KEY | ASC | ASCQ
constexpr unsigned SENSE_NO_SENSE               = 0x000000;
constexpr unsigned SENSE_NOT_READY              = 0x020400;
constexpr unsigned SENSE_MEDIUM_NOT_PRESENT     = 0x023a00;
constexpr unsigned SENSE_UNRECOVERED_READ_ERROR = 0x031100;
constexpr unsigned SENSE_WRITE_FAULT            = 0x040300;
constexpr unsigned SENSE_INVALID_COMMAND_CODE   = 0x052000;
constexpr unsigned SENSE_ILLEGAL_BLOCK_ADDRESS  = 0x052100;
constexpr unsigned SENSE_INVALID_LUN            = 0x052500;
constexpr unsigned SENSE_POWER_ON               = 0x062900;
constexpr unsigned SENSE_WRITE_PROTECT          = 0x072700;
constexpr unsigned SENSE_MESSAGE_REJECT_ERROR   = 0x0b4300;
constexpr unsigned SENSE_INITIATOR_DETECTED_ERR = 0x0b4800;
constexpr unsigned SENSE_ILLEGAL_MESSAGE        = 0x0b4900;

// Message
constexpr byte MSG_COMMAND_COMPLETE       = 0x00;
constexpr byte MSG_INITIATOR_DETECT_ERROR = 0x05;
constexpr byte MSG_ABORT                  = 0x06;
constexpr byte MSG_REJECT                 = 0x07;
constexpr byte MSG_NO_OPERATION           = 0x08;
constexpr byte MSG_PARITY_ERROR           = 0x09;
constexpr byte MSG_BUS_DEVICE_RESET       = 0x0c;

// Status
constexpr byte ST_GOOD            = 0;
constexpr byte ST_CHECK_CONDITION = 2;
constexpr byte ST_BUSY            = 8;

// Device type
constexpr byte DT_DirectAccess     = 0x00;
constexpr byte DT_SequencialAccess = 0x01;
constexpr byte DT_Printer          = 0x02;
constexpr byte DT_Processor        = 0x03;
constexpr byte DT_WriteOnce        = 0x04;
constexpr byte DT_CDROM            = 0x05;
constexpr byte DT_Scanner          = 0x06;
constexpr byte DT_OpticalMemory    = 0x07;
constexpr byte DT_MediaChanger     = 0x08;
constexpr byte DT_Communications   = 0x09;
constexpr byte DT_Undefined        = 0x1f;

enum Phase {
	UNDEFINED, // used in MB89532
	BUS_FREE,
	ARBITRATION,
	SELECTION,
	RESELECTION,
	COMMAND,
	EXECUTE,
	DATA_IN,
	DATA_OUT,
	STATUS,
	MSG_OUT,
	MSG_IN,
};

} // namespace openmsx::SCSI

#endif
