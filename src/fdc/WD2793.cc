// $Id$

#include "WD2793.hh"


WD2793::WD2793(MSXConfig::Device *config)
	: FDC(config)
{
	reset();
	timePerStep[0]=6;	// in MSX a 1MHz clock is used!
	timePerStep[1]=12;
	timePerStep[2]=20;
	timePerStep[3]=30;
}

WD2793::~WD2793()
{
}

void WD2793::reset()
{
  PRT_DEBUG("WD2793::reset()");
  statusReg=0;
  trackReg=0;
  dataReg=0;

  current_track=0;
  current_sector=0;
  current_side=0;
  stepSpeed=0;
  directionIn=true;

  current_drive=0;
  motor_drive[0]=0;
  motor_drive[1]=0;
  //motorStartTime[0]=0;
  //motorStartTime[1]=0;
  
  // According to the specs it nows issues a RestoreCommando (0x03) Afterwards
  // the stepping rate can still be changed so that the remaining steps of the
  // restorecommand can go faster. On an MSX this time can be ignored since the
  // bootstrap of the MSX takes MUCH longer then an ,even failing,
  // Restorecommand.
  commandReg=0x03;
  sectorReg=0x01;
  DRQ=false;
  INTRQ=false;
  //statusReg bit 7 (Not Ready status) is already reset
}

void WD2793::setDriveSelect(byte value,const EmuTime &time)
{
  switch (value & 3){
  case 0:
  case 2:
  current_drive=0;
  break;
  case 1:
  current_drive=1;
  break;
  case 3:
  current_drive=255; //no drive selected
  };

  //PRT_DEBUG("motor is "<<(int)motor_drive<<" drive "<<(int)current_drive);
}

void WD2793::setMotor(byte value,const EmuTime &time)
{
  if (current_drive != 255 ){
  	if (motor_drive[current_drive] != value){
	  //if already in current state this is skipped
	  motorStartTime[current_drive]=time;
	}
	motor_drive[current_drive]=value;
  };
}

//actually not used, maybe for GUI ?
byte WD2793::getDriveSelect(const EmuTime &time)
{
  return current_drive;
}

//actually not used ,maybe for GUI ?
byte WD2793::getMotor(const EmuTime &time)
{
  return (current_drive<4)?motor_drive[current_drive]:0;
}

byte WD2793::getDTRQ(const EmuTime &time)
{
  return dataAvailable?1:0 ;
}

byte WD2793::getIRQ(const EmuTime &time)
{
  return INTRQ?1:0;
  if (INTRQ){
    INTRQ=false;
    return 1;
  } else {
    return 0;
  }
}

void WD2793::setSideSelect(byte value,const EmuTime &time)
{
  current_side=value&1;
}

byte WD2793::getSideSelect(const EmuTime &time)
{
  return current_side;
}

void WD2793::setCommandReg(byte value,const EmuTime &time)
{
	commandReg = value;
	statusReg |= 1 ;// set status on Busy
	INTRQ=false;
	DRQ=false;
	// commandEnd = commandStart = time;
	commandEnd = time;
	
	//First we set some flags from the lower four bits of the command
	Cflag		= value & 2 ;
	stepSpeed	= value & 3 ;
	Eflag = Vflag	= value & 4 ;
	hflag = Sflag	= value & 8 ;
	mflag = Tflag	= value & 16;
	// above code could by executed always with no if's
	// what-so-ever. Since the flags are always written during
	// a new command and only applicable 
	// for the current command
	// some confusion about Eflag/Vflag see below
	// flags for the Force Interrupt are ignored for now.
	
	switch (value & 0xF0){
	  case 0x00: //restore
	  	PRT_DEBUG("FDC command: restore");

		commandEnd += (current_track * timePerStep[stepSpeed]);
		if (Vflag) commandEnd += 15; //Head setting time

		// according to page 1-100 last alinea, however not sure so ommited
		// if (Eflag) commandEnd += 15;
		current_track = 0;
		trackReg=0;
		directionIn = false;
		// TODO Ask ricardo about his "timeout" for a restore driveb ?
		statusReg &= 254 ;// reset status on Busy
	    break;
	    
	  case 0x10: //seek
	  	PRT_DEBUG("FDC command: seek");
	  	//PRT_DEBUG("before: track "<<(int)trackReg<<",data "<<(int)dataReg<<",cur "<<(int)current_track);
	    if ( trackReg != dataReg ){
	      // It could be that the current track isn't the one indicated by the dataReg-sectorReg
	      // so we calculated the steps the FDC will send to get the two regs the same 
	      // and add/distract this from the real current track
	      byte steps;
	      if (trackReg<dataReg){
		steps = dataReg-trackReg ;
		current_track += steps;   	
		directionIn=true;
	      } else {
		steps = trackReg-dataReg;
		current_track -= steps;
		directionIn=false;
	      }

	      trackReg = dataReg;

	      commandEnd += (steps * timePerStep[stepSpeed]);
	      if (Vflag) commandEnd += 15; //Head setting time

	      //TODO actually verify
	  	//PRT_DEBUG("after : track "<<(int)trackReg<<",data "<<(int)dataReg<<",cur "<<(int)current_track);
		statusReg &= 254 ;// reset status on Busy
	    };
	    break;

	  case 0x20: //step
	  case 0x30: //step (Update trackRegister)
	  	PRT_DEBUG("FDC command: step (Tflag "<<(int)Tflag<<")");
		if (directionIn){
		  current_track++;
		  if (Tflag) trackReg++;
		} else {
		  current_track--;
		  if (Tflag) trackReg--;
		};

		commandEnd += timePerStep[stepSpeed];
		if (Vflag) commandEnd += 15; //Head setting time

		statusReg &= 254 ;// reset status on Busy
	    break;

	  case 0x40: //step-in
	  case 0x50: //step-in (Update trackRegister)
	  	PRT_DEBUG("FDC command: step in (Tflag "<<(int)Tflag<<")");
		current_track++;
		directionIn = true;
		if (Tflag) trackReg++;

		commandEnd += timePerStep[stepSpeed];
		if (Vflag) commandEnd += 15; //Head setting time

		statusReg &= 254 ;// reset status on Busy
	    break;

	  case 0x60: //step-out
	  case 0x70: //step-out (Update trackRegister)
	  	PRT_DEBUG("FDC command: step out (Tflag "<<(int)Tflag<<")");
		current_track--;  // TODO Specs don't say what happens if track was already 0 !!
		directionIn = false;
		if (Tflag) trackReg++;

		commandEnd += timePerStep[stepSpeed];
		if (Vflag) commandEnd += 15; //Head setting time

		statusReg &= 254 ;// reset status on Busy
	    break;

	  case 0x80: //read sector
	  case 0x90: //read sector (multi)
	  	PRT_DEBUG("FDC command: read sector");
		INTRQ=false;
		DRQ=false;
		statusReg &= 0x01 ;// reset lost data,record not found & status bits 5 & 6
		//statusReg &= 0x 	// fake ready, fake CRC error, fake record t1=copy DRQ Type :-)
	  	dataCurrent=0;
		dataAvailable=512; // TODO should come from sector header !!!
		try {
			getBackEnd(current_drive)->read(current_track, trackReg,
			              sectorReg, current_side, 512, dataBuffer);
			statusReg |= 2; DRQ=true; // data ready to be read
		} catch (MSXException &e) {
		  statusReg |= 2; DRQ=true; // TODO data not ready because read error
		}
	    break;

	  case 0xA0: // write sector
	  case 0xB0: // write sector (multi)
	  	PRT_DEBUG("FDC command: write sector");
		INTRQ=false;
		DRQ=false;
		statusReg &= 0x01 ;// reset lost data,record not found & status bits 5 & 6
	  	dataCurrent=0;
		dataAvailable=512; // TODO should come from sector header !!!
		statusReg |= 2; DRQ=true; // data ready to be written
	    break;

	  case 0xC0: //Read Address
	  	PRT_DEBUG("FDC command: read address");
	      PRT_INFO("FDC command not yet implemented ");
	    break;
	  case 0xD0: //Force interrupt
	      PRT_DEBUG("FDC command: Force interrupt statusregister "<<(int)statusReg);
	      statusReg &= 254 ;// reset status on Busy
	    break;
	  case 0xE0: //read track
	       PRT_DEBUG("FDC command: read track");
	      PRT_INFO("FDC command not yet implemented ");
	    break;
	  case 0xF0: //write track
	      PRT_DEBUG("FDC command: write track");
	      statusReg &= 0x01 ;// reset lost data,record not found & status bits 5 & 6
	      statusReg |= 2; DRQ=true;
	      //PRT_INFO("FDC command not yet implemented ");
	    break;
	}
}

byte WD2793::getStatusReg(const EmuTime &time)
{
	if ( (commandReg & 0x80) == 0){
		//Type I command so bit 1 should be the index pulse
		if ( current_drive < 2 ){
		   int ticks = motorStartTime[current_drive].getTicksTill(time);
		   if ( ticks >= 200) ticks -= 200 * ( int (ticks/200) );
		   
		   //TODO: check on a real MSX how long this indexmark is visible 
		   // According to Ricardo it is simply a sensor that is mapped
		   // onto this bit that sees if the index mark is passing by
		   // or not. (The little gap in the metal plate of te disk)
		   if (ticks < 20) statusReg |= 2;
		}
	};
	//statusReg &= 254 ;// reset status on Busy 
	//TODO this hould be time dependend !!!
	// like in : if (time>=endTime) statusReg &= 254;
	if (time>=commandEnd) statusReg &= 254;
	PRT_DEBUG("statusReg is "<<(int)statusReg);
	return statusReg;
}

void WD2793::setTrackReg(byte value,const EmuTime &time)
{
  trackReg=value;
};
byte WD2793::getTrackReg(const EmuTime &time)
{
  return trackReg;
};

void WD2793::setSectorReg(byte value,const EmuTime &time)
{
  sectorReg=value;
};
byte WD2793::getSectorReg(const EmuTime &time)
{
  return sectorReg;
};

void WD2793::setDataReg(byte value, const EmuTime &time)
{
	dataReg=value; // TODO is this also true in case of sector write ? Not so according to ASM of brMSX
	if ((commandReg&0xE0)==0xA0){ // WRITE SECTOR
	  dataBuffer[dataCurrent]=value;
	  dataCurrent++;
	  dataAvailable--;
	  if ( dataAvailable == 0 ){
		PRT_DEBUG("Now we call the backend to write a sector");
		try {
			getBackEnd(current_drive)->write(current_track, trackReg,
			              sectorReg, current_side, 512, dataBuffer);
			// If we wait too long we should also write a partialy filled sector
			// ofcourse and set the correct status bits !!
			statusReg &= 0x7D ;// reset status on Busy(=bit7) reset DRQ bit(=bit1)
			DRQ=false;
			if (mflag==0) {
				//TODO verify this !
				INTRQ=true;
			}
			dataCurrent=0;
			dataAvailable=512; // TODO should come from sector header !!!
		} catch (MSXException &e) {
			// Backend couldn't write data
		}
	  }
	} else if ((commandReg&0xF0)==0xF0){ // WRITE TRACK
	    PRT_DEBUG("WD2793 WRITE TRACK value "<<(int)value);
	    switch ( value ){
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
		PRT_DEBUG("CRC generator initializing in MFM,write A1 ?");
		break;
	      case 0xF7:
		PRT_DEBUG("two CRC characters");
		break;
	      default:
	        //Normal write to track
		break;
	    }
	    	//shouldn't been done here !!
		statusReg &= 0x7D ;// reset status on Busy(=bit7) reset DRQ bit(=bit1)
	    /*
	    if (indexmark){
		statusReg &= 0x7D ;// reset status on Busy(=bit7) reset DRQ bit(=bit1)
		INTRQ=true;
		DRQ=false; 
	    }
	    */
	}

}

byte WD2793::getDataReg(const EmuTime &time)
{
  if ((commandReg&0xE0)==0x80){ // READ SECTOR
    dataReg=dataBuffer[dataCurrent];
    dataCurrent++;
    dataAvailable--;
    if ( dataAvailable == 0 ){
      statusReg &= 0x7D ;// reset status on Busy(=bit7) reset DRQ bit(=bit1)
      DRQ=false;
      if (mflag==0) INTRQ=true;
      PRT_DEBUG("Now we terminate the read sector command or skip to next sector if multi set");
    }
  }
  return dataReg;
}
