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
static const byte OP_TEST_UNIT_READY       = 0x00;
static const byte OP_REZERO_UNIT           = 0x01;
static const byte OP_REQUEST_SENSE         = 0x03;
static const byte OP_FORMAT_UNIT           = 0x04;
static const byte OP_REASSIGN_BLOCKS       = 0x07;
static const byte OP_READ6                 = 0x08;
static const byte OP_WRITE6                = 0x0A;
static const byte OP_SEEK6                 = 0x0B;
static const byte OP_INQUIRY               = 0x12;
static const byte OP_RESERVE_UNIT          = 0x16;
static const byte OP_RELEASE_UNIT          = 0x17;
static const byte OP_MODE_SENSE            = 0x1A;
static const byte OP_START_STOP_UNIT       = 0x1B;
static const byte OP_SEND_DIAGNOSTIC       = 0x1D;

// Group 1: 10bytes cdb
static const byte OP_GROUP1                = 0x20;
static const byte OP_READ_CAPACITY         = 0x25;
static const byte OP_READ10                = 0x28;
static const byte OP_WRITE10               = 0x2A;
static const byte OP_SEEK10                = 0x2B;

static const byte OP_GROUP2                = 0x40;
static const byte OP_CHANGE_DEFINITION     = 0x40;
static const byte OP_READ_SUB_CHANNEL      = 0x42;
static const byte OP_READ_TOC              = 0x43;
static const byte OP_READ_HEADER           = 0x44;
static const byte OP_PLAY_AUDIO            = 0x45;
static const byte OP_PLAY_AUDIO_MSF        = 0x47;
static const byte OP_PLAY_TRACK_INDEX      = 0x48;
static const byte OP_PLAY_TRACK_RELATIVE   = 0x49;
static const byte OP_PAUSE_RESUME          = 0x4B;

static const byte OP_PLAY_AUDIO12          = 0xA5;
static const byte OP_READ12                = 0xA8;
static const byte OP_PLAY_TRACK_RELATIVE12 = 0xA9;
static const byte OP_READ_CD_MSF           = 0xB9;
static const byte OP_READ_CD               = 0xBE;

// Sense data                               KEY | ASC | ASCQ
static const unsigned SENSE_NO_SENSE               = 0x000000;
static const unsigned SENSE_NOT_READY              = 0x020400;
static const unsigned SENSE_MEDIUM_NOT_PRESENT     = 0x023a00;
static const unsigned SENSE_UNRECOVERED_READ_ERROR = 0x031100;
static const unsigned SENSE_WRITE_FAULT            = 0x040300;
static const unsigned SENSE_INVALID_COMMAND_CODE   = 0x052000;
static const unsigned SENSE_ILLEGAL_BLOCK_ADDRESS  = 0x052100;
static const unsigned SENSE_INVALID_LUN            = 0x052500;
static const unsigned SENSE_POWER_ON               = 0x062900;
static const unsigned SENSE_WRITE_PROTECT          = 0x072700;
static const unsigned SENSE_MESSAGE_REJECT_ERROR   = 0x0b4300;
static const unsigned SENSE_INITIATOR_DETECTED_ERR = 0x0b4800;
static const unsigned SENSE_ILLEGAL_MESSAGE        = 0x0b4900;

// Message
static const byte MSG_COMMAND_COMPLETE       = 0x00;
static const byte MSG_INITIATOR_DETECT_ERROR = 0x05;
static const byte MSG_ABORT                  = 0x06;
static const byte MSG_REJECT                 = 0x07;
static const byte MSG_NO_OPERATION           = 0x08;
static const byte MSG_PARITY_ERROR           = 0x09;
static const byte MSG_BUS_DEVICE_RESET       = 0x0c;

// Status
static const byte ST_GOOD            = 0;
static const byte ST_CHECK_CONDITION = 2;
static const byte ST_BUSY            = 8;

// Device type
static const byte DT_DirectAccess     = 0x00;
static const byte DT_SequencialAccess = 0x01;
static const byte DT_Printer          = 0x02;
static const byte DT_Processor        = 0x03;
static const byte DT_WriteOnce        = 0x04;
static const byte DT_CDROM            = 0x05;
static const byte DT_Scanner          = 0x06;
static const byte DT_OpticalMemory    = 0x07;
static const byte DT_MediaChanger     = 0x08;
static const byte DT_Communications   = 0x09;
static const byte DT_Undefined        = 0x1f;

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
