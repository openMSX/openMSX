#include "WD2793.hh"
#include "DiskDrive.hh"
#include "CliComm.hh"
#include "Clock.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <iostream>

namespace openmsx {

// Status register
constexpr int BUSY             = 0x01;
constexpr int INDEX            = 0x02;
constexpr int S_DRQ            = 0x02;
constexpr int TRACK00          = 0x04;
constexpr int LOST_DATA        = 0x04;
constexpr int CRC_ERROR        = 0x08;
constexpr int SEEK_ERROR       = 0x10;
constexpr int RECORD_NOT_FOUND = 0x10;
constexpr int HEAD_LOADED      = 0x20;
constexpr int RECORD_TYPE      = 0x20;
constexpr int WRITE_PROTECTED  = 0x40;
constexpr int NOT_READY        = 0x80;

// Command register
constexpr int STEP_SPEED = 0x03;
constexpr int V_FLAG     = 0x04;
constexpr int E_FLAG     = 0x04;
constexpr int H_FLAG     = 0x08;
constexpr int T_FLAG     = 0x10;
constexpr int M_FLAG     = 0x10;
constexpr int A0_FLAG    = 0x01;
constexpr int N2R_IRQ    = 0x01;
constexpr int R2N_IRQ    = 0x02;
constexpr int IDX_IRQ    = 0x04;
constexpr int IMM_IRQ    = 0x08;

// HLD/HLT timing constants
constexpr auto IDLE = EmuDuration::sec(3);

/** This class has emulation for WD1770, WD1793, WD2793. Though at the moment
 * the only emulated difference between WD1770 and WD{12}793 is that WD1770
 * has no ready input signal. (E.g. we don't emulate the WD1770 motor out
 * signal yet).
 */
WD2793::WD2793(Scheduler& scheduler_, DiskDrive& drive_, CliComm& cliComm_,
               EmuTime::param time, bool isWD1770_)
	: Schedulable(scheduler_)
	, drive(drive_)
	, cliComm(cliComm_)
	, drqTime(EmuTime::infinity())
	, irqTime(EmuTime::infinity())
	, pulse5(EmuTime::infinity())
	, hldTime(EmuTime::infinity()) // HLD=false
	, isWD1770(isWD1770_)
{
	// avoid (harmless) UMR in serialize()
	dataCurrent = 0;
	dataAvailable = 0;
	dataOutReg = 0;
	dataRegWritten = false;
	lastWasA1 = false;
	lastWasCRC = false;
	commandReg = 0;
	setDrqRate(RawTrack::STANDARD_SIZE);

	reset(time);
}

void WD2793::reset(EmuTime::param time)
{
	removeSyncPoint();
	fsmState = FSM_NONE;

	statusReg = 0;
	trackReg = 0;
	dataReg = 0;
	directionIn = true;

	drqTime.reset(EmuTime::infinity()); // DRQ = false
	irqTime = EmuTime::infinity();      // INTRQ = false;
	immediateIRQ = false;

	// Execute Restore command
	sectorReg = 0x01;
	setCommandReg(0x03, time);
}

bool WD2793::getDTRQ(EmuTime::param time) const
{
	return peekDTRQ(time);
}

bool WD2793::peekDTRQ(EmuTime::param time) const
{
	return time >= drqTime.getTime();
}

void WD2793::setDrqRate(unsigned trackLength)
{
	drqTime.setFreq(trackLength * DiskDrive::ROTATIONS_PER_SECOND);
}

bool WD2793::getIRQ(EmuTime::param time) const
{
	return peekIRQ(time);
}

bool WD2793::peekIRQ(EmuTime::param time) const
{
	return immediateIRQ || (irqTime <= time);
}

bool WD2793::isReady() const
{
	// The WD1770 has no ready input signal (instead that pin is replaced
	// by a motor-on/off output pin).
	return drive.isDiskInserted() || isWD1770;
}

void WD2793::setCommandReg(byte value, EmuTime::param time)
{
	if (((commandReg & 0xE0) == 0xA0) || // write sector
	    ((commandReg & 0xF0) == 0xF0)) { // write track
		// If a write sector/track command is cancelled, still flush
		// the partially written data to disk.
		try {
			drive.flushTrack();
		} catch (MSXException&) {
			// ignore
		}
	}

	removeSyncPoint();

	commandReg = value;
	irqTime = EmuTime::infinity(); // INTRQ = false;
	switch (commandReg & 0xF0) {
		case 0x00: // restore
		case 0x10: // seek
		case 0x20: // step
		case 0x30: // step (Update trackRegister)
		case 0x40: // step-in
		case 0x50: // step-in (Update trackRegister)
		case 0x60: // step-out
		case 0x70: // step-out (Update trackRegister)
			startType1Cmd(time);
			break;

		case 0x80: // read sector
		case 0x90: // read sector (multi)
		case 0xA0: // write sector
		case 0xB0: // write sector (multi)
			startType2Cmd(time);
			break;

		case 0xC0: // Read Address
		case 0xE0: // read track
		case 0xF0: // write track
			startType3Cmd(time);
			break;

		case 0xD0: // Force interrupt
			startType4Cmd(time);
			break;
	}
}

byte WD2793::getStatusReg(EmuTime::param time)
{
	if (((commandReg & 0x80) == 0) || ((commandReg & 0xF0) == 0xD0)) {
		// Type I or type IV command
		statusReg &= ~(INDEX | TRACK00 | HEAD_LOADED | WRITE_PROTECTED);
		if (drive.indexPulse(time)) {
			statusReg |=  INDEX;
		}
		if (drive.isTrack00()) {
			statusReg |=  TRACK00;
		}
		if ((hldTime <= time) && (time < (hldTime + IDLE))) {
			statusReg |=  HEAD_LOADED;
		}
		if (drive.isWriteProtected()) {
			statusReg |=  WRITE_PROTECTED;
		}
	} else {
		// Not type I command so bit 1 should be DRQ
		if (getDTRQ(time)) {
			statusReg |=  S_DRQ;
		} else {
			statusReg &= ~S_DRQ;
		}
	}

	if (isReady()) {
		statusReg &= ~NOT_READY;
	} else {
		statusReg |=  NOT_READY;
	}

	// Reset INTRQ only if it's not scheduled to turn on in the future.
	if (irqTime <= time) { // if (INTRQ == true)
		irqTime = EmuTime::infinity(); // INTRQ = false;
	}

	return statusReg;
}

byte WD2793::peekStatusReg(EmuTime::param time) const
{
	// TODO implement proper peek?
	return const_cast<WD2793*>(this)->getStatusReg(time);
}

void WD2793::setTrackReg(byte value, EmuTime::param /*time*/)
{
	trackReg = value;
}

byte WD2793::getTrackReg(EmuTime::param time) const
{
	return peekTrackReg(time);
}

byte WD2793::peekTrackReg(EmuTime::param /*time*/) const
{
	return trackReg;
}

void WD2793::setSectorReg(byte value, EmuTime::param /*time*/)
{
	sectorReg = value;
}

byte WD2793::getSectorReg(EmuTime::param time) const
{
	return peekSectorReg(time);
}

byte WD2793::peekSectorReg(EmuTime::param /*time*/) const
{
	return sectorReg;
}

void WD2793::setDataReg(byte value, EmuTime::param time)
{
	dataReg = value;

	if (!getDTRQ(time)) return;
	assert(statusReg & BUSY);

	if (((commandReg & 0xE0) == 0xA0) || // write sector
	    ((commandReg & 0xF0) == 0xF0)) { // write track
		dataRegWritten = true;
		drqTime.reset(EmuTime::infinity()); // DRQ = false
	}
}

byte WD2793::getDataReg(EmuTime::param time)
{
	if ((((commandReg & 0xE0) == 0x80) ||   // read sector
	     ((commandReg & 0xF0) == 0xC0) ||   // read address
	     ((commandReg & 0xF0) == 0xE0)) &&  // read track
	    getDTRQ(time)) {
		assert(statusReg & BUSY);

		dataReg = drive.readTrackByte(dataCurrent++);
		crc.update(dataReg);
		dataAvailable--;
		drqTime += 1; // time when the next byte will be available
		while (dataAvailable && unlikely(getDTRQ(time))) {
			statusReg |= LOST_DATA;
			dataReg = drive.readTrackByte(dataCurrent++);
			crc.update(dataReg);
			dataAvailable--;
			drqTime += 1;
		}
		assert(!dataAvailable || !getDTRQ(time));
		if (dataAvailable == 0) {
			if ((commandReg & 0xE0) == 0x80) {
				// read sector
				// update crc status flag
				word diskCrc  = 256 * drive.readTrackByte(dataCurrent++);
				     diskCrc +=       drive.readTrackByte(dataCurrent++);
				if (diskCrc == crc.getValue()) {
					statusReg &= ~CRC_ERROR;
				} else {
					statusReg |=  CRC_ERROR;
				}
				if (sectorInfo.deleted) {
					// TODO datasheet isn't clear about this:
					// Set this flag at the end of the
					// command or as soon as the marker is
					// encountered on the disk?
					statusReg |= RECORD_TYPE;
				}
				if (!(commandReg & M_FLAG)) {
					endCmd(time);
				} else {
					// multi sector read, wait for the next sector
					drqTime.reset(EmuTime::infinity()); // DRQ = false
					sectorReg++;
					type2Loaded(time);
				}
			} else {
				// read track, read address
				if ((commandReg & 0xF0) == 0xE0) { // read track
					drive.invalidateWd2793ReadTrackQuirk();
				}
				if ((commandReg & 0xF0) == 0xC0) { // read address
					if (sectorInfo.addrCrcErr) {
						statusReg |=  CRC_ERROR;
					} else {
						statusReg &= ~CRC_ERROR;
					}
				}
				endCmd(time);
			}
		}
	}
	return dataReg;
}

byte WD2793::peekDataReg(EmuTime::param time) const
{
	if ((((commandReg & 0xE0) == 0x80) ||   // read sector
	     ((commandReg & 0xF0) == 0xC0) ||   // read address
	     ((commandReg & 0xF0) == 0xE0)) &&  // read track
	    peekDTRQ(time)) {
		return drive.readTrackByte(dataCurrent);
	} else {
		return dataReg;
	}
}


void WD2793::schedule(FSMState state, EmuTime::param time)
{
	assert(!pendingSyncPoint());
	fsmState = state;
	setSyncPoint(time);
}

void WD2793::executeUntil(EmuTime::param time)
{
	FSMState state = fsmState;
	fsmState = FSM_NONE;
	switch (state) {
		case FSM_SEEK:
			if ((commandReg & 0x80) == 0x00) {
				// Type I command
				seekNext(time);
			}
			break;
		case FSM_TYPE2_LOADED:
			if ((commandReg & 0xC0) == 0x80)  {
				// Type II command
				type2Loaded(time);
			}
			break;
		case FSM_TYPE2_NOT_FOUND:
			if ((commandReg & 0xC0) == 0x80)  {
				// Type II command
				type2NotFound(time);
			}
			break;
		case FSM_TYPE2_ROTATED:
			if ((commandReg & 0xC0) == 0x80)  {
				// Type II command
				type2Rotated(time);
			}
			break;
		case FSM_CHECK_WRITE:
			if ((commandReg & 0xE0) == 0xA0) {
				// write sector command
				checkStartWrite(time);
			}
			break;
		case FSM_PRE_WRITE_SECTOR:
			if ((commandReg & 0xE0) == 0xA0) {
				// write sector command
				preWriteSector(time);
			}
			break;
		case FSM_WRITE_SECTOR:
			if ((commandReg & 0xE0) == 0xA0) {
				// write sector command
				writeSectorData(time);
			}
			break;
		case FSM_POST_WRITE_SECTOR:
			if ((commandReg & 0xE0) == 0xA0) {
				// write sector command
				postWriteSector(time);
			}
			break;
		case FSM_TYPE3_LOADED:
			if (((commandReg & 0xC0) == 0xC0) &&
			    ((commandReg & 0xF0) != 0xD0)) {
				// Type III command
				type3Loaded(time);
			}
			break;
		case FSM_TYPE3_ROTATED:
			if (((commandReg & 0xC0) == 0xC0) &&
			    ((commandReg & 0xF0) != 0xD0)) {
				// Type III command
				type3Rotated(time);
			}
			break;
		case FSM_WRITE_TRACK:
			if ((commandReg & 0xF0) == 0xF0) {
				// write track command
				writeTrackData(time);
			}
			break;
		case FSM_READ_TRACK:
			if ((commandReg & 0xF0) == 0xE0) {
				// read track command
				drive.invalidateWd2793ReadTrackQuirk();
				endCmd(time); // TODO check this (e.g. DRQ)
			}
			break;
		default:
			UNREACHABLE;
	}
}

void WD2793::startType1Cmd(EmuTime::param time)
{
	statusReg &= ~(SEEK_ERROR | CRC_ERROR);
	statusReg |= BUSY;

	if (commandReg & H_FLAG) {
		// Activate HLD, WD2793 now waits for the HLT response. But on
		// all MSX machines I checked HLT is just stubbed to +5V. So
		// from a WD2793 point of view the head is loaded immediately.
		hldTime = time;
	} else {
		// deactivate HLD
		hldTime = EmuTime::infinity();
	}

	switch (commandReg & 0xF0) {
		case 0x00: // restore
			trackReg = 0xFF;
			dataReg  = 0x00;
			seek(time);
			break;

		case 0x10: // seek
			seek(time);
			break;

		case 0x20: // step
		case 0x30: // step (Update trackRegister)
			step(time);
			break;

		case 0x40: // step-in
		case 0x50: // step-in (Update trackRegister)
			directionIn = true;
			step(time);
			break;

		case 0x60: // step-out
		case 0x70: // step-out (Update trackRegister)
			directionIn = false;
			step(time);
			break;
	}
}

void WD2793::seek(EmuTime::param time)
{
	if (trackReg == dataReg) {
		endType1Cmd(time);
	} else {
		directionIn = (dataReg > trackReg);
		step(time);
	}
}

void WD2793::step(EmuTime::param time)
{
	static constexpr EmuDuration timePerStep[4] = {
		// in case a 1MHz clock is used (as in MSX)
		EmuDuration::msec( 6),
		EmuDuration::msec(12),
		EmuDuration::msec(20),
		EmuDuration::msec(30),
	};

	if ((commandReg & T_FLAG) || ((commandReg & 0xE0) == 0x00)) {
		// Restore or seek  or  T_FLAG
		if (directionIn) {
			trackReg++;
		} else {
			trackReg--;
		}
	}
	if (!directionIn && drive.isTrack00()) {
		trackReg = 0;
		endType1Cmd(time);
	} else {
		drive.step(directionIn, time);
		schedule(FSM_SEEK, time + timePerStep[commandReg & STEP_SPEED]);
	}
}

void WD2793::seekNext(EmuTime::param time)
{
	if ((commandReg & 0xE0) == 0x00) {
		// Restore or seek
		seek(time);
	} else {
		endType1Cmd(time);
	}
}

void WD2793::endType1Cmd(EmuTime::param time)
{
	if (commandReg & V_FLAG) {
		// verify sequence
		// TODO verify sequence
	}
	endCmd(time);
}


void WD2793::startType2Cmd(EmuTime::param time)
{
	statusReg &= ~(LOST_DATA   | RECORD_NOT_FOUND |
	               RECORD_TYPE | WRITE_PROTECTED);
	statusReg |= BUSY;
	dataRegWritten = false;

	if (!isReady()) {
		endCmd(time);
	} else {
		// WD2795/WD2797 would now set SSO output
		hldTime = time; // see comment in startType1Cmd

		if (commandReg & E_FLAG) {
			schedule(FSM_TYPE2_LOADED,
			         time + EmuDuration::msec(30)); // when 1MHz clock
		} else {
			type2Loaded(time);
		}
	}
}

void WD2793::type2Loaded(EmuTime::param time)
{
	if (((commandReg & 0xE0) == 0xA0) && (drive.isWriteProtected())) {
		// write command and write protected
		statusReg |= WRITE_PROTECTED;
		endCmd(time);
		return;
	}

	pulse5 = drive.getTimeTillIndexPulse(time, 5);
	type2Search(time);
}

void WD2793::type2Search(EmuTime::param time)
{
	assert(time < pulse5);
	// Locate (next) sector on disk.
	try {
		setDrqRate(drive.getTrackLength());
		EmuTime next = drive.getNextSector(time, sectorInfo);
		if (next < pulse5) {
			// Wait till sector is actually rotated under head
			schedule(FSM_TYPE2_ROTATED, next);
			return;
		}
	} catch (MSXException& /*e*/) {
		// nothing
	}
	// Sector not found in 5 revolutions (or read error),
	// schedule to give a RECORD_NOT_FOUND error
	if (pulse5 < EmuTime::infinity()) {
		schedule(FSM_TYPE2_NOT_FOUND, pulse5);
	} else {
		// Drive not rotating. How does a real WD293 handle this?
		type2NotFound(time);
	}
}

void WD2793::type2Rotated(EmuTime::param time)
{
	// The CRC status bit should only toggle after the disk has rotated
	if (sectorInfo.addrCrcErr) {
		statusReg |=  CRC_ERROR;
	} else {
		statusReg &= ~CRC_ERROR;
	}
	if ((sectorInfo.addrCrcErr) ||
	    (sectorInfo.track  != trackReg) ||
	    (sectorInfo.sector != sectorReg)) {
		// TODO implement (optional) head compare
		// not the sector we were looking for, continue searching
		type2Search(time);
		return;
	}
	if (sectorInfo.dataIdx == -1) {
		// Sector header without accompanying data block.
		// TODO we should actually wait for the disk to rotate before
		// we can check this.
		type2Search(time);
		return;
	}

	// Ok, found matching sector.
	switch (commandReg & 0xE0) {
	case 0x80: // read sector  or  read sector multi
		startReadSector(time);
		break;

	case 0xA0: // write sector  or  write sector multi
		startWriteSector(time);
		break;
	}
}

void WD2793::type2NotFound(EmuTime::param time)
{
	statusReg |= RECORD_NOT_FOUND;
	endCmd(time);
}

void WD2793::startReadSector(EmuTime::param time)
{
	if (sectorInfo.deleted) {
		crc.init({0xA1, 0xA1, 0xA1, 0xF8});
	} else {
		crc.init({0xA1, 0xA1, 0xA1, 0xFB});
	}
	unsigned trackLength = drive.getTrackLength();
	int tmp = sectorInfo.dataIdx - sectorInfo.addrIdx;
	unsigned gapLength = (tmp >= 0) ? tmp : (tmp + trackLength);
	assert(gapLength < trackLength);
	drqTime.reset(time);
	drqTime += gapLength + 1 + 1; // (first) byte can be read in a moment
	dataCurrent = sectorInfo.dataIdx;

	// Get sectorsize from disk: 128, 256, 512 or 1024 bytes
	// Verified on real WD2793:
	//   sizecode=255 results in a sector size of 1024 bytes,
	// This suggests the WD2793 only looks at the lower 2 bits.
	dataAvailable = 128 << (sectorInfo.sizeCode & 3);
}

void WD2793::startWriteSector(EmuTime::param time)
{
	// At the current moment in time, the 'FE' byte in the address mark
	// is located under the drive head (because the DMK format points to
	// the 'FE' byte in the address header). After this byte there still
	// follow the C,H,R,N and 2 crc bytes. So the address header ends in
	// 7 bytes.
	// - After 2 more bytes the WD2793 will activate DRQ.
	// - 8 bytes later the WD2793 will check that the CPU has send the
	//   first byte (if not the command will be aborted without any writes
	//   to the disk, not even gap or data mark bytes).
	// - after a pause of 12 bytes, the WD2793 will write 12 zero bytes,
	//   followed by the 4 bytes data header (A1 A1 A1 FB).
	// - Next the WD2793 write the actual data bytes. At this moment it
	//   will also activate DRQ to receive the 2nd byte from the CPU.
	//
	// Note that between the 1st and 2nd activation of DRQ is a longer
	// duration than between all later DRQ activations. The write-sector
	// routine in Microsol_CDX-2 depends on this.

	drqTime.reset(time);
	drqTime += 7 + 2; // activate DRQ 2 bytes after end of address header

	// 8 bytes later, the WD2793 will check whether the CPU wrote the
	// first byte.
	schedule(FSM_CHECK_WRITE, drqTime + 8);
}

void WD2793::checkStartWrite(EmuTime::param time)
{
	// By now the CPU should already have written the first byte, otherwise
	// the write sector command doesn't even start.
	if (!dataRegWritten) {
		statusReg |= LOST_DATA;
		endCmd(time);
		return;
	}

	// From this point onwards the FDC will write a full sector to the disk
	// (including markers and CRC). If the CPU doesn't supply data bytes in
	// a timely manner, the missing bytes are filled with zero.
	//
	// But it is possible to cancel the write sector command to only write
	// a partial sector (e.g. without CRC bytes). See cbsfox' comment in
	// this forum thread:
	//    https://www.msx.org/forum/msx-talk/software/pdi-to-dmk-using-dsk-pro-104-with-openmsx
	dataCurrent = sectorInfo.addrIdx
	            + 6 // C H R N CRC1 CRC2
		    + 22;

	// pause 12 bytes
	drqTime.reset(time);
	schedule(FSM_PRE_WRITE_SECTOR, drqTime + 12);
	drqTime.reset(EmuTime::infinity()); // DRQ = false
	dataAvailable = 16; // 12+4 bytes pre-data
}

void WD2793::preWriteSector(EmuTime::param time)
{
	try {
		--dataAvailable;
		if (dataAvailable > 0) {
			if (dataAvailable >= 4) {
				// write 12 zero-bytes
				drive.writeTrackByte(dataCurrent++, 0x00);
			} else {
				// followed by 3x A1-bytes
				drive.writeTrackByte(dataCurrent++, 0xA1);
			}
			drqTime.reset(time);
			schedule(FSM_PRE_WRITE_SECTOR, drqTime + 1);
			drqTime.reset(EmuTime::infinity()); // DRQ = false
		} else {
			// and finally a single F8/FB byte
			crc.init({0xA1, 0xA1, 0xA1});
			byte mark = (commandReg & A0_FLAG) ? 0xF8 : 0xFB;
			drive.writeTrackByte(dataCurrent++, mark);
			crc.update(mark);

			// Pre-data is finished. Next start writing the actual data bytes
			dataOutReg = dataReg;
			dataRegWritten = false;
			dataAvailable = 128 << (sectorInfo.sizeCode & 3); // see comment in startReadSector()

			// Re-activate DRQ
			drqTime.reset(time);

			// Moment in time when first data byte gets written
			schedule(FSM_WRITE_SECTOR, drqTime + 1);
		}
	} catch (MSXException&) {
		statusReg |= NOT_READY; // TODO which status bit should be set?
		endCmd(time);
	}
}

void WD2793::writeSectorData(EmuTime::param time)
{
	try {
		// Write data byte
		drive.writeTrackByte(dataCurrent++, dataOutReg);
		crc.update(dataOutReg);
		--dataAvailable;

		if (dataAvailable > 0) {
			if (dataRegWritten) {
				dataOutReg = dataReg;
				dataRegWritten = false;
			} else {
				dataOutReg = 0;
				statusReg |= LOST_DATA;
			}
			// Re-activate DRQ
			drqTime.reset(time);

			// Moment in time when next data byte gets written
			schedule(FSM_WRITE_SECTOR, drqTime + 1);
		} else {
			// Next write post-part
			dataAvailable = 3;
			drqTime.reset(time);
			schedule(FSM_POST_WRITE_SECTOR, drqTime + 1);
			drqTime.reset(EmuTime::infinity()); // DRQ = false
		}
	} catch (MSXException&) {
		statusReg |= NOT_READY; // TODO which status bit should be set?
		endCmd(time);
	}
}

void WD2793::postWriteSector(EmuTime::param time)
{
	try {
		--dataAvailable;
		if (dataAvailable > 0) {
			// write 2 CRC bytes (big endian)
			byte val = (dataAvailable == 2) ? (crc.getValue() >> 8)
							: (crc.getValue() & 0xFF);
			drive.writeTrackByte(dataCurrent++, val);
			drqTime.reset(time);
			schedule(FSM_POST_WRITE_SECTOR, drqTime + 1);
			drqTime.reset(EmuTime::infinity()); // DRQ = false
		} else {
			// write one byte of 0xFE
			drive.writeTrackByte(dataCurrent++, 0xFE);

			// flush sector (actually full track) to disk.
			drive.flushTrack();

			if (!(commandReg & M_FLAG)) {
				endCmd(time);
			} else {
				// multi sector write, wait for next sector
				drqTime.reset(EmuTime::infinity()); // DRQ = false
				sectorReg++;
				type2Loaded(time);
			}
		}
	} catch (MSXException&) {
		// e.g. triggers when a different drive was selected during write
		statusReg |= NOT_READY; // TODO which status bit should be set?
		endCmd(time);
	}
}


void WD2793::startType3Cmd(EmuTime::param time)
{
	statusReg &= ~(LOST_DATA | RECORD_NOT_FOUND | RECORD_TYPE);
	statusReg |= BUSY;

	if (!isReady()) {
		endCmd(time);
	} else {
		if ((commandReg & 0xF0) == 0xF0) { // write track
			// immediately activate DRQ
			drqTime.reset(time); // DRQ = true
		}

		hldTime = time; // see comment in startType1Cmd
		// WD2795/WD2797 would now set SSO output

		if (commandReg & E_FLAG) {
			schedule(FSM_TYPE3_LOADED,
			         time + EmuDuration::msec(30)); // when 1MHz clock
		} else {
			type3Loaded(time);
		}
	}
}

void WD2793::type3Loaded(EmuTime::param time)
{
	// TODO TG43 update
	if (((commandReg & 0xF0) == 0xF0) && (drive.isWriteProtected())) {
		// write track command and write protected
		statusReg |= WRITE_PROTECTED;
		endCmd(time);
		return;
	}

	EmuTime next(EmuTime::dummy());
	if ((commandReg & 0xF0) == 0xC0) {
		// read address
		try {
			// wait till next sector header
			setDrqRate(drive.getTrackLength());
			next = drive.getNextSector(time, sectorInfo);
			if (next == EmuTime::infinity()) {
				// TODO wait for 5 revolutions
				statusReg |= RECORD_NOT_FOUND;
				endCmd(time);
				return;
			}
			dataCurrent = sectorInfo.addrIdx;
			dataAvailable = 6;
			sectorReg = drive.readTrackByte(dataCurrent);
		} catch (MSXException&) {
			// read addr failed
			statusReg |= RECORD_NOT_FOUND;
			endCmd(time);
			return;
		}
	} else {
		// read/write track
		// wait till next index pulse
		next = drive.getTimeTillIndexPulse(time);
		if (next == EmuTime::infinity()) {
			// drive became not ready since the command was started,
			// how does a real WD2793 handle this?
			endCmd(time);
			return;
		}
	}
	schedule(FSM_TYPE3_ROTATED, next);
}

void WD2793::type3Rotated(EmuTime::param time)
{
	switch (commandReg & 0xF0) {
	case 0xC0: // read Address
		readAddressCmd(time);
		break;
	case 0xE0: // read track
		readTrackCmd(time);
		break;
	case 0xF0: // write track
		startWriteTrack(time);
		break;
	}
}

void WD2793::readAddressCmd(EmuTime::param time)
{
	drqTime.reset(time);
	drqTime += 1; // (first) byte can be read in a moment
}

void WD2793::readTrackCmd(EmuTime::param time)
{
	try {
		unsigned trackLength = drive.getTrackLength();
		drive.applyWd2793ReadTrackQuirk();
		setDrqRate(trackLength);
		dataCurrent = 0;
		dataAvailable = trackLength;
		drqTime.reset(time);

		// Stop command at next index pulse
		schedule(FSM_READ_TRACK, drqTime + dataAvailable);

		drqTime += 1; // (first) byte can be read in a moment
	} catch (MSXException&) {
		// read track failed, TODO status bits?
		drive.invalidateWd2793ReadTrackQuirk();
		endCmd(time);
	}
}

void WD2793::startWriteTrack(EmuTime::param time)
{
	// By now the CPU should already have written the first byte, otherwise
	// the write track command doesn't even start.
	if (!dataRegWritten) {
		statusReg |= LOST_DATA;
		endCmd(time);
		return;
	}
	try {
		unsigned trackLength = drive.getTrackLength();
		setDrqRate(trackLength);
		dataCurrent = 0;
		dataAvailable = trackLength;
		lastWasA1 = false;
		lastWasCRC = false;
		dataOutReg = dataReg;
		dataRegWritten = false;
		drqTime.reset(time); // DRQ = true

		// Moment in time when first track byte gets written
		schedule(FSM_WRITE_TRACK, drqTime + 1);
	} catch (MSXException& /*e*/) {
		endCmd(time);
	}
}

void WD2793::writeTrackData(EmuTime::param time)
{
	try {
		bool prevA1 = lastWasA1;
		lastWasA1 = false;

		// handle chars with special meaning
		bool idam = false;
		if (lastWasCRC) {
			// 2nd CRC byte, don't transform
			lastWasCRC = false;
		} else if (dataOutReg == 0xF5) {
			// write A1 with missing clock transitions
			dataOutReg = 0xA1;
			lastWasA1 = true;
			// Initialize CRC: the calculated CRC value
			// includes the 3 A1 bytes. So when starting
			// from the initial value 0xffff, we should not
			// re-initialize the CRC value on the 2nd and
			// 3rd A1 byte. Though what we do instead is on
			// each A1 byte initialize the value as if
			// there were already 2 A1 bytes written.
			crc.init({0xA1, 0xA1});
		} else if (dataOutReg == 0xF6) {
			// write C2 with missing clock transitions
			dataOutReg = 0xC2;
		} else if (dataOutReg == 0xF7) {
			// write 2 CRC bytes, big endian
			dataOutReg = crc.getValue() >> 8; // high byte
			lastWasCRC = true;
		} else if (dataOutReg == 0xFE) {
			// Record locations of 0xA1 (with missing clock
			// transition) followed by 0xFE. The FE byte has
			// no special meaning for the WD2793 itself,
			// but it does for the DMK file format.
			if (prevA1) idam = true;
		}
		// actually write (transformed) byte
		drive.writeTrackByte(dataCurrent++, dataOutReg, idam);
		if (!lastWasCRC) {
			crc.update(dataOutReg);
		}
		--dataAvailable;

		if (dataAvailable > 0) {
			drqTime.reset(time); // DRQ = true

			// Moment in time when next track byte gets written
			schedule(FSM_WRITE_TRACK, drqTime + 1);

			// prepare next byte
			if (!lastWasCRC) {
				if (dataRegWritten) {
					dataOutReg = dataReg;
					dataRegWritten = false;
				} else {
					dataOutReg = 0;
					statusReg |= LOST_DATA;
				}
			} else {
				dataOutReg = crc.getValue() & 0xFF; // low byte
				// don't re-activate DRQ for 2nd byte of CRC
				drqTime.reset(EmuTime::infinity()); // DRQ = false
			}
		} else {
			// Write track done
			drive.flushTrack();
			endCmd(time);
		}
	} catch (MSXException&) {
		statusReg |= NOT_READY; // TODO which status bit should be set?
		endCmd(time);
	}
}

void WD2793::startType4Cmd(EmuTime::param time)
{
	// Force interrupt
	byte flags = commandReg & 0x0F;
	if (flags & (N2R_IRQ | R2N_IRQ)) {
		// all flags not yet supported
		#ifdef DEBUG
		std::cerr << "WD2793 type 4 cmd, unimplemented bits " << int(flags) << '\n';
		#endif
	}

	if (flags == 0x00) {
		immediateIRQ = false;
	}
	if ((flags & IDX_IRQ) && isReady()) {
		irqTime = drive.getTimeTillIndexPulse(time);
	} else {
		assert(irqTime == EmuTime::infinity()); // INTRQ = false
	}
	if (flags & IMM_IRQ) {
		immediateIRQ = true;
	}

	drqTime.reset(EmuTime::infinity()); // DRQ = false
	statusReg &= ~BUSY; // reset status on Busy
}

void WD2793::endCmd(EmuTime::param time)
{
	if ((hldTime <= time) && (time < (hldTime + IDLE))) {
		// HLD was active, start timeout period
		// Real WD2793 waits for 15 index pulses. We approximate that
		// here by waiting for 3s.
		hldTime = time;
	}
	drqTime.reset(EmuTime::infinity()); // DRQ   = false
	irqTime = EmuTime::zero();          // INTRQ = true;
	statusReg &= ~BUSY;
}


static constexpr std::initializer_list<enum_string<WD2793::FSMState>> fsmStateInfo = {
	{ "NONE",              WD2793::FSM_NONE },
	{ "SEEK",              WD2793::FSM_SEEK },
	{ "TYPE2_LOADED",      WD2793::FSM_TYPE2_LOADED },
	{ "TYPE2_NOT_FOUND",   WD2793::FSM_TYPE2_NOT_FOUND },
	{ "TYPE2_ROTATED",     WD2793::FSM_TYPE2_ROTATED },
	{ "CHECK_WRITE",       WD2793::FSM_CHECK_WRITE },
	{ "PRE_WRITE_SECTOR",  WD2793::FSM_PRE_WRITE_SECTOR },
	{ "WRITE_SECTOR",      WD2793::FSM_WRITE_SECTOR },
	{ "POST_WRITE_SECTOR", WD2793::FSM_POST_WRITE_SECTOR },
	{ "TYPE3_LOADED",      WD2793::FSM_TYPE3_LOADED },
	{ "TYPE3_ROTATED",     WD2793::FSM_TYPE3_ROTATED },
	{ "WRITE_TRACK",       WD2793::FSM_WRITE_TRACK },
	{ "READ_TRACK",        WD2793::FSM_READ_TRACK },
	{ "IDX_IRQ",           WD2793::FSM_IDX_IRQ },
	// for bw-compat savestate
	{ "TYPE2_WAIT_LOAD",   WD2793::FSM_TYPE2_LOADED }, // was FSM_TYPE2_WAIT_LOAD
	{ "TYPE3_WAIT_LOAD",   WD2793::FSM_TYPE3_LOADED }, // was FSM_TYPE3_WAIT_LOAD
};
SERIALIZE_ENUM(WD2793::FSMState, fsmStateInfo);

// version 1: initial version
// version 2: removed members: commandStart, DRQTimer, DRQ, transferring, formatting
//            added member: drqTime (has different semantics than DRQTimer)
//            also the timing of the data-transfer commands (read/write sector
//            and write track) has changed. So this could result in replay-sync
//            errors.
//            (Also the enum FSMState has changed, but that's not a problem.)
// version 3: Added members 'crc' and 'lastWasA1'.
//            Replaced 'dataBuffer' with 'trackData'. We don't attempt to migrate
//            the old 'dataBuffer' content to 'trackData' (doing so would be
//            quite difficult). This means that old savestates that were in the
//            middle of a sector/track read/write command probably won't work
//            correctly anymore. We do give a warning on this.
// version 4: changed type of drqTime from Clock to DynamicClock
// version 5: added 'pulse5' and 'sectorInfo'
// version 6: no layout changes, only added new enum value 'FSM_CHECK_WRITE'
// version 7: replaced 'bool INTRQ' with 'EmuTime irqTime'
// version 8: removed 'userData' from Schedulable
// version 9: added 'trackDataValid'
// version 10: removed 'trackData' and 'trackDataValid' (moved to RealDrive)
// version 11: added 'dataOutReg', 'dataRegWritten', 'lastWasCRC'
// version 12: added 'hldTime'
template<typename Archive>
void WD2793::serialize(Archive& ar, unsigned version)
{
	EmuTime bw_irqTime = EmuTime::zero();
	if (ar.versionAtLeast(version, 8)) {
		ar.template serializeBase<Schedulable>(*this);
	} else {
		constexpr int SCHED_FSM     = 0;
		constexpr int SCHED_IDX_IRQ = 1;
		assert(Archive::IS_LOADER);
		removeSyncPoint();
		for (auto& old : Schedulable::serializeBW(ar)) {
			if (old.userData == SCHED_FSM) {
				setSyncPoint(old.time);
			} else if (old.userData == SCHED_IDX_IRQ) {
				bw_irqTime = old.time;
			}
		}
	}

	ar.serialize("fsmState",      fsmState,
	             "statusReg",     statusReg,
	             "commandReg",    commandReg,
	             "sectorReg",     sectorReg,
	             "trackReg",      trackReg,
	             "dataReg",       dataReg,

	             "directionIn",   directionIn,
	             "immediateIRQ",  immediateIRQ,

	             "dataCurrent",   dataCurrent,
	             "dataAvailable", dataAvailable);

	if (ar.versionAtLeast(version, 2)) {
		if (ar.versionAtLeast(version, 4)) {
			ar.serialize("drqTime", drqTime);
		} else {
			assert(Archive::IS_LOADER);
			Clock<6250 * 5> c(EmuTime::dummy());
			ar.serialize("drqTime", c);
			drqTime.reset(c.getTime());
			drqTime.setFreq(6250 * 5);
		}
	} else {
		assert(Archive::IS_LOADER);
		//ar.serialize("commandStart", commandStart,
		//             "DRQTimer",     DRQTimer,
		//             "DRQ",          DRQ,
		//             "transferring", transferring,
		//             "formatting",   formatting);
		drqTime.reset(EmuTime::infinity());
	}

	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("lastWasA1", lastWasA1);
		word crcVal = crc.getValue();
		ar.serialize("crc", crcVal);
		crc.init(crcVal);
	}

	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("pulse5",     pulse5,
		             "sectorInfo", sectorInfo);
	} else {
		// leave pulse5 at EmuTime::infinity()
		// leave sectorInfo uninitialized
	}

	if (ar.versionAtLeast(version, 7)) {
		ar.serialize("irqTime", irqTime);
	} else {
		assert(Archive::IS_LOADER);
		bool INTRQ = false; // dummy init to avoid warning
		ar.serialize("INTRQ", INTRQ);
		irqTime = INTRQ ? EmuTime::zero() : EmuTime::infinity();
		if (bw_irqTime != EmuTime::zero()) {
			irqTime = bw_irqTime;
		}
	}

	if (ar.versionAtLeast(version, 11)) {
		ar.serialize("dataOutReg",     dataOutReg,
		             "dataRegWritten", dataRegWritten,
		             "lastWasCRC",     lastWasCRC);
	} else {
		assert(Archive::IS_LOADER);
		dataOutReg = dataReg;
		dataRegWritten = false;
		lastWasCRC = false;
	}

	if (ar.versionBelow(version, 11)) {
		assert(Archive::IS_LOADER);
		// version 9->10: 'trackData' moved from FDC to RealDrive
		// version 10->11: write commands are different
		if (statusReg & BUSY) {
			cliComm.printWarning(
				"Loading an old savestate that has an "
				"in-progress WD2793 command. This is not "
				"fully backwards-compatible and can cause "
				"wrong emulation behavior.");
		}
	}

	if (ar.versionAtLeast(version, 12)) {
		ar.serialize("hldTime", hldTime);
	} else {
		if (statusReg & BUSY) {
			hldTime = getCurrentTime();
		} else {
			hldTime = EmuTime::infinity();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(WD2793);

} // namespace openmsx
