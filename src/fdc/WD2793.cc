// $Id$

#include "WD2793.hh"
#include "DiskDrive.hh"
#include "Scheduler.hh"


WD2793::WD2793(DiskDrive *drive, const EmuTime &time)
{
	this->drive = drive;
	reset(time);
}

WD2793::~WD2793()
{
}

void WD2793::reset(const EmuTime &time)
{
	PRT_DEBUG("WD2793::reset()");
	statusReg = 0;
	trackReg = 0;
	dataReg = 0;

	directionIn = true;

	// According to the specs it nows issues a RestoreCommando (0x03)
	// Afterwards the stepping rate can still be changed so that the
	// remaining steps of the restorecommand can go faster. On an MSX this
	// time can be ignored since the bootstrap of the MSX takes MUCH longer
	// then an, even failing, Restorecommand
	commandReg = 0x03;
	sectorReg = 0x01;
	DRQ = false;
	INTRQ = false;
	immediateIRQ = false;
	//statusReg bit 7 (Not Ready status) is already reset
}

bool WD2793::getDTRQ(const EmuTime &time)
{
	if (((commandReg & 0xF0) == 0xF0) &&
	    (statusReg & BUSY)) {
		// WRITE TRACK && status busy
		int ticks = DRQTime.getTicksTill(time);
		if (ticks >= 15) { 
			// writing a byte during format takes +/- 33 us
			// according to tech data but on trubor fdc docs it
			// state 15 us to get data ready
			// TODO check for clockspeed of used diagram in wdc
			//      docs this could explain mistake
			DRQTime -= 15;
			DRQ = true;
		}
	}
	return DRQ;
}

bool WD2793::getIRQ(const EmuTime &time)
{
	return INTRQ;
}

void WD2793::setIRQ()
{
	INTRQ = true;
}

void WD2793::resetIRQ()
{
	INTRQ = immediateIRQ;
}

void WD2793::setCommandReg(byte value, const EmuTime &time)
{
	commandReg = value;
	resetIRQ();
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

byte WD2793::getStatusReg(const EmuTime &time)
{
	if (((commandReg & 0x80) == 0) || ((commandReg & 0xF0) == 0xD0)) {
		// Type I or type IV command 
		if (drive->indexPulse(time)) {
			statusReg |=  INDEX;
		} else {
			statusReg &= ~INDEX;
		}
		if (drive->track00(time)) {
			statusReg |=  TRACK00;
		} else {
			statusReg &= ~TRACK00;
		}
		if (drive->headLoaded(time)) {
			statusReg |=  HEAD_LOADED;
		} else {
			statusReg &= ~HEAD_LOADED;
		}
		if (drive->writeProtected()) {
			statusReg |=  WRITE_PROTECTED;
		} else {
			statusReg &= ~WRITE_PROTECTED;
		}
	} else {
		// Not type I command so bit 1 should be DRQ
		if (getDTRQ(time)) {
			statusReg |=  S_DRQ;
		} else {
			statusReg &= ~S_DRQ;
		}
	}
	
	if (drive->ready()) {
		statusReg &= ~NOT_READY;
	} else {
		statusReg |=  NOT_READY;
	}
	
	resetIRQ();
	return statusReg;
}

void WD2793::setTrackReg(byte value,const EmuTime &time)
{
	trackReg = value;
}

byte WD2793::getTrackReg(const EmuTime &time)
{
	return trackReg;
}

void WD2793::setSectorReg(byte value,const EmuTime &time)
{
	sectorReg = value;
}

byte WD2793::getSectorReg(const EmuTime &time)
{
	return sectorReg;
}

void WD2793::setDataReg(byte value, const EmuTime &time)
{
	// TODO Is this also true in case of sector write?
	//      Not so according to ASM of brMSX
	dataReg = value;
	if ((commandReg & 0xE0) == 0xA0) {
		// WRITE SECTOR
		dataBuffer[dataCurrent] = value;
		dataCurrent++;
		dataAvailable--;
		if (dataAvailable == 0) {
			PRT_DEBUG("FDC: Now we call the backend to write a sector");
			try {
				drive->write(sectorReg, dataBuffer,
				             onDiskTrack, onDiskSector,
					     onDiskSide, onDiskSize);
				assert(onDiskTrack == trackReg);
				assert(onDiskSize == 512);
				// If we wait too long we should also write a
				// partialy filled sector ofcourse and set the
				// correct status bits!
				statusReg &= ~0x03;	// reset status on Busy and DRQ
				if (!(commandReg & M_FLAG)) {
					//TODO verify this !
					setIRQ();
					DRQ = false;
				}
				dataCurrent = 0;
				dataAvailable = onDiskSize;
			} catch (MSXException &e) {
				// Backend couldn't write data
			}
		}
	} else if ((commandReg & 0xF0) == 0xF0) {
		// WRITE TRACK
		//PRT_DEBUG("WD2793 WRITE TRACK value "<<std::hex<<
		//          (int)value<<std::dec);
		//DRQ related timing
		DRQ = false;
		DRQTime = time;
		
		//indexmark related timing
		int pulses = drive->indexPulseCount(commandStart, time);
		switch (pulses) {
		case 0: // no index pulse yet
			break;
		case 1: // first index pulse passed
			drive->writeTrackData(value);
			break;
		case 2: // next indexpulse passed
			dataAvailable = 0; //return correct DTR
			statusReg &= ~0x03;	// reset status on Busy and DRQ
			DRQ = false;
			setIRQ();
			dataCurrent = 0;
			break;
		}
		/* followin switch stement belongs in the backend, since
		 * we do not know how the actual diskimage stores the
		 * data. It might simply drop all the extra CRC/header
		 * stuff and just use some of the switches to actually
		 * simply write a 512 bytes sector.
		 *
		 * However, timing should be done here :-\
		 *

		 switch (value) {
		 case 0xFE:
		 case 0xFD:
		 case 0xFC:
		 case 0xFB:
		 case 0xFA:
		 case 0xF9:
		 case 0xF8:
			PRT_DEBUG("CRC generator initializing");
			break;
		 case 0xF6:
			PRT_DEBUG("write C2 ?");
			break;
		 case 0xF5:
			PRT_DEBUG("CRC generator initializing in MFM, write A1?");
			break;
		 case 0xF7:
			PRT_DEBUG("two CRC characters");
			break;
		 default:
			//Normal write to track
			break;
		 }
		 //shouldn't been done here !!
		 statusReg &= ~0x03;	// reset status on Busy and DRQ
		 */


		/*
		   if (indexmark) {
		   statusReg &= ~0x03;	// reset status on Busy and DRQ
		   setIRQ();
		   DRQ = false; 
		   }
		 */
	}
}

byte WD2793::getDataReg(const EmuTime &time)
{
	if (((commandReg & 0xE0) == 0x80) && (statusReg & BUSY)) {
		// READ SECTOR
		dataReg = dataBuffer[dataCurrent];
		dataCurrent++;
		dataAvailable--;
		if (dataAvailable == 0) {
			if (!(commandReg & M_FLAG)) {
				statusReg &= ~0x03;	// reset status on Busy and DRQ
				DRQ = false;
				setIRQ();
				PRT_DEBUG("FDC: Now we terminate the read sector command");
			} else {
				// TODO ceck in tech data (or on real machine)
				// if implementation multi sector read is
				// correct, since this is programmed by hart.
				sectorReg++;
				tryToReadSector();
			}
		}
	}
	return dataReg;
}

void WD2793::tryToReadSector(void)
{
	try {
		drive->read(sectorReg, dataBuffer,
		            onDiskTrack, onDiskSector, onDiskSide, onDiskSize);
		assert(onDiskTrack == trackReg);
		assert(onDiskSize == 512);
		dataCurrent = 0;
		dataAvailable = onDiskSize;
		DRQ = true;	// data ready to be read
	} catch (MSXException &e) {
		PRT_DEBUG("FDC: read sector failed: " << e.desc);
		DRQ = false;	// TODO data not ready (read error)
		statusReg = 0;	// reset flags
	}
}


void WD2793::executeUntilEmuTime(const EmuTime &time, int state)
{
	switch((FSMState)state) {
		case FSM_SEEK:
			if ((commandReg & 0x80) == 0x00) {
				// Type I command
				seekNext(time);
			}
			break;
		case FSM_TYPE2_WAIT_LOAD:
			if ((commandReg & 0xC0) == 0x80)  {
				// Type II command
				type2WaitLoad(time);
			}
			break;
		default:
			assert(false);
	}
}

void WD2793::startType1Cmd(const EmuTime &time)
{
	statusReg &= ~(SEEK | CRC);
	statusReg |= BUSY;
	DRQ = false;

	drive->setHeadLoaded(commandReg & H_FLAG, time);

	switch(commandReg & 0xF0) {
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

void WD2793::seek(const EmuTime &time)
{
	if (trackReg == dataReg) {
		endType1Cmd(time);
	} else {
		directionIn = (dataReg > trackReg);
		step(time);
	}
}

void WD2793::step(const EmuTime &time)
{
	const int timePerStep[4] = {
		// in ms, in case a 1MHz clock is used (as in MSX)
		6, 12, 20, 30
	};

	if (commandReg & T_FLAG) {
		if (directionIn) {
			trackReg++;
		} else {
			trackReg--;
		}
	}
	if (!directionIn && drive->track00(time)) {
		trackReg = 0;
		endType1Cmd(time);
	} else {
		drive->step(directionIn, time);
		EmuTimeFreq<1000> next(time);	// ms
		next += timePerStep[commandReg & STEP_SPEED];
		Scheduler::instance()->setSyncPoint(next, this, FSM_SEEK);
	}
}

void WD2793::seekNext(const EmuTime &time)
{
	if ((commandReg & 0xE0) == 0x00) {
		// Restore or seek
		seek(time);
	} else {
		endType1Cmd(time);
	}
}

void WD2793::endType1Cmd(const EmuTime &time)
{
	if (commandReg & V_FLAG) {
		// verify sequence
		
		// TODO verify sequence
	} 
	endCmd();
}


void WD2793::startType2Cmd(const EmuTime &time)
{
	statusReg &= 0x01;	// reset lost data,record not found & status bits 5 & 6
	statusReg |= 1;	// set status on Busy

	if (!drive->ready()) {
		endCmd();
	} else {
		// WD279[5|7] would now set SSO output
	
		drive->setHeadLoaded(true, time);

		if (commandReg & E_FLAG) {
			EmuTimeFreq<1000> next(time);	// ms
			next += 30;
			Scheduler::instance()->setSyncPoint(next, this, FSM_TYPE2_WAIT_LOAD);
		} else {
			type2WaitLoad(time);
		}
	}
}

void WD2793::type2WaitLoad(const EmuTime& time)
{
	// TODO wait till head loaded
	
	if (((commandReg & 0xE0) == 0xA0) && (drive->writeProtected())) {
		// write command and write protected
		statusReg |= WRITE_PROTECTED;
		endCmd();
	} else {
		switch (commandReg & 0xF0) {
			case 0x80: //read sector
			case 0x90: //read sector (multi)
				tryToReadSector();
				break;

			case 0xA0: // write sector
			case 0xB0: // write sector (multi)
				dataCurrent = 0;
				dataAvailable = 512;	// TODO should come from sector header
				DRQ = true;	// data ready to be written
				break;
		}
	}
}

void WD2793::startType3Cmd(const EmuTime &time)
{
	commandStart = time;
	statusReg |= BUSY;	// set status on Busy
	DRQ = false;

	switch (commandReg & 0xF0) {
	case 0xC0: //Read Address
		PRT_DEBUG("FDC command: read address");
		assert(false);	// not yet implemented
		break;
		
	case 0xE0: //read track
		PRT_DEBUG("FDC command: read track");
		assert(false);	// not yet implemented
		break;
		
	case 0xF0: //write track
		PRT_DEBUG("FDC command: write track");
		statusReg &= 0x01;	// reset lost data,record not found & status bits 5 & 6
		DRQ = true;
		drive->initWriteTrack(); 
		PRT_DEBUG("WD2793: initWriteTrack()");

		//PRT_INFO("FDC command not yet implemented ");
		//CommandController::instance()->executeCommand(std::string("cpudebug"));
		//statusReg &= ~BUSY;	// reset status on Busy
		// Variables below are a not-completely-correct hack:
		// Correct behavior would indicate that one waits until the
		// next indexmark before the first byte is written and that
		// from the command stays active until the next indexmark.
		
		// TODO wait for indexPulse

		DRQTime = time;
		break;
	}
}

void WD2793::startType4Cmd(const EmuTime &time)
{
	// Force interrupt
	PRT_DEBUG("FDC command: Force interrupt");
	
	byte flags = commandReg & 0x0F;
	assert((flags & 0x07) == 0x00); // all flags not yet supported

	if (flags == 0x00) {
		immediateIRQ = false;
	}
	if (flags & IMM_IRQ) {
		immediateIRQ = true;
	}
	
	DRQ = false;
	statusReg &= ~BUSY;	// reset status on Busy
}

void WD2793::endCmd()
{
	setIRQ();
	statusReg &= ~BUSY;
}

