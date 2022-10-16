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

#include <cstdint>

namespace openmsx::SCSI {

// Group 0: 6bytes cdb
inline constexpr uint8_t OP_TEST_UNIT_READY       = 0x00;
inline constexpr uint8_t OP_REZERO_UNIT           = 0x01;
inline constexpr uint8_t OP_REQUEST_SENSE         = 0x03;
inline constexpr uint8_t OP_FORMAT_UNIT           = 0x04;
inline constexpr uint8_t OP_REASSIGN_BLOCKS       = 0x07;
inline constexpr uint8_t OP_READ6                 = 0x08;
inline constexpr uint8_t OP_WRITE6                = 0x0A;
inline constexpr uint8_t OP_SEEK6                 = 0x0B;
inline constexpr uint8_t OP_INQUIRY               = 0x12;
inline constexpr uint8_t OP_RESERVE_UNIT          = 0x16;
inline constexpr uint8_t OP_RELEASE_UNIT          = 0x17;
inline constexpr uint8_t OP_MODE_SENSE            = 0x1A;
inline constexpr uint8_t OP_START_STOP_UNIT       = 0x1B;
inline constexpr uint8_t OP_SEND_DIAGNOSTIC       = 0x1D;

// Group 1: 10bytes cdb
inline constexpr uint8_t OP_GROUP1                = 0x20;
inline constexpr uint8_t OP_READ_CAPACITY         = 0x25;
inline constexpr uint8_t OP_READ10                = 0x28;
inline constexpr uint8_t OP_WRITE10               = 0x2A;
inline constexpr uint8_t OP_SEEK10                = 0x2B;

inline constexpr uint8_t OP_GROUP2                = 0x40;
inline constexpr uint8_t OP_CHANGE_DEFINITION     = 0x40;
inline constexpr uint8_t OP_READ_SUB_CHANNEL      = 0x42;
inline constexpr uint8_t OP_READ_TOC              = 0x43;
inline constexpr uint8_t OP_READ_HEADER           = 0x44;
inline constexpr uint8_t OP_PLAY_AUDIO            = 0x45;
inline constexpr uint8_t OP_PLAY_AUDIO_MSF        = 0x47;
inline constexpr uint8_t OP_PLAY_TRACK_INDEX      = 0x48;
inline constexpr uint8_t OP_PLAY_TRACK_RELATIVE   = 0x49;
inline constexpr uint8_t OP_PAUSE_RESUME          = 0x4B;

inline constexpr uint8_t OP_PLAY_AUDIO12          = 0xA5;
inline constexpr uint8_t OP_READ12                = 0xA8;
inline constexpr uint8_t OP_PLAY_TRACK_RELATIVE12 = 0xA9;
inline constexpr uint8_t OP_READ_CD_MSF           = 0xB9;
inline constexpr uint8_t OP_READ_CD               = 0xBE;

// Sense data                               KEY | ASC | ASCQ
inline constexpr uint32_t SENSE_NO_SENSE               = 0x000000;
inline constexpr uint32_t SENSE_NOT_READY              = 0x020400;
inline constexpr uint32_t SENSE_MEDIUM_NOT_PRESENT     = 0x023a00;
inline constexpr uint32_t SENSE_UNRECOVERED_READ_ERROR = 0x031100;
inline constexpr uint32_t SENSE_WRITE_FAULT            = 0x040300;
inline constexpr uint32_t SENSE_INVALID_COMMAND_CODE   = 0x052000;
inline constexpr uint32_t SENSE_ILLEGAL_BLOCK_ADDRESS  = 0x052100;
inline constexpr uint32_t SENSE_INVALID_LUN            = 0x052500;
inline constexpr uint32_t SENSE_POWER_ON               = 0x062900;
inline constexpr uint32_t SENSE_WRITE_PROTECT          = 0x072700;
inline constexpr uint32_t SENSE_MESSAGE_REJECT_ERROR   = 0x0b4300;
inline constexpr uint32_t SENSE_INITIATOR_DETECTED_ERR = 0x0b4800;
inline constexpr uint32_t SENSE_ILLEGAL_MESSAGE        = 0x0b4900;

// Message
inline constexpr uint8_t MSG_COMMAND_COMPLETE       = 0x00;
inline constexpr uint8_t MSG_INITIATOR_DETECT_ERROR = 0x05;
inline constexpr uint8_t MSG_ABORT                  = 0x06;
inline constexpr uint8_t MSG_REJECT                 = 0x07;
inline constexpr uint8_t MSG_NO_OPERATION           = 0x08;
inline constexpr uint8_t MSG_PARITY_ERROR           = 0x09;
inline constexpr uint8_t MSG_BUS_DEVICE_RESET       = 0x0c;

// Status
inline constexpr uint8_t ST_GOOD            = 0;
inline constexpr uint8_t ST_CHECK_CONDITION = 2;
inline constexpr uint8_t ST_BUSY            = 8;

// Device type
inline constexpr uint8_t DT_DirectAccess     = 0x00;
inline constexpr uint8_t DT_SequencialAccess = 0x01;
inline constexpr uint8_t DT_Printer          = 0x02;
inline constexpr uint8_t DT_Processor        = 0x03;
inline constexpr uint8_t DT_WriteOnce        = 0x04;
inline constexpr uint8_t DT_CDROM            = 0x05;
inline constexpr uint8_t DT_Scanner          = 0x06;
inline constexpr uint8_t DT_OpticalMemory    = 0x07;
inline constexpr uint8_t DT_MediaChanger     = 0x08;
inline constexpr uint8_t DT_Communications   = 0x09;
inline constexpr uint8_t DT_Undefined        = 0x1f;

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
