#include "FDC2793.hh"
#include "FDC_DSK.hh"

FDC2793::FDC2793(MSXConfig::Device *config) 
  : FDC(config)
{
  PRT_DEBUG("instantiating an FDC2793 object..");
  try {
    std::string back = config->getParameter("backend");
    PRT_DEBUG( "backend: " << back );
    if ( strcmp(back.c_str(),"DSK") == 0 ) {
      backend=new FDC_DSK(config);
    }
  } catch(MSXConfig::Exception& e) {
    // No chip type found
  }
  reset();

}

FDC2793::~FDC2793()
{
  PRT_DEBUG("Destructing an FDC2793 object..");
}

void FDC2793::reset()
{
  PRT_DEBUG("FDC2793::reset()");
  statusReg=0;
  trackReg=0;
  dataReg=0;

  current_track=0;
  current_sector=0;
  stepSpeed=0;
  directionIn=true;
  
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
};
byte FDC2793::getDataReg(const EmuTime &time)
{
	if ( (commandReg & 0x80) == 0){
		//Type I command so bit 1 should be the index pulse
		
	}
	return statusReg;
}

void FDC2793::setTrackReg(byte value,const EmuTime &time)
{
  trackReg=value;
};
byte FDC2793::getTrackReg(const EmuTime &time)
{
  return trackReg;
};

void FDC2793::setSectorReg(byte value,const EmuTime &time)
{
  sectorReg=value;
};
byte FDC2793::getSectorReg(const EmuTime &time)
{
  return sectorReg;
};


