// $Id$
 
#include "SCC.hh"

SCC::SCC()
{
	PRT_DEBUG("instantiating an SCC object");
//}
//
//void SCC::init()
//{
  //Register itself as a soundevice
  // TODO setVolume(21000);	// TODO find a good value and put it in config file
  int bufSize = Mixer::instance()->registerSound(this);
  buffer = new int[bufSize];
  setInternalMute(false);  // set muted

  setVolume(25000);
  reset();
}

SCC::~SCC()
{
	PRT_DEBUG("Destructing an SCC object");
}
byte SCC::readMemInterface(byte address,const EmuTime &time)
{
	return memInterface[address];
}
void SCC::setChipMode(ChipMode chip)
{
  if (currentChipMode==chip)
    return;
  // lower 128 bytes are always the same
  switch (chip){

    case SCC_Real:
      //Simply mask upper 128 bytes
      for (int i=128 ; i<=255 ; i++){
	memInterface[i]=255;
      };
      break;
      
    case SCC_Compatible:
      //32 bytes Frequency Volume data
      getFreqVol(0x80);
      //32 bytes waveform five
      for (int i=0 ; i<=31 ; i++)
	memInterface[0xA0+i]=wave[4][i];
      //32 bytes deform register
      getDeform(0xC0);
      //no function area
      for (int i=0xE0 ; i<=0xFF ; i++)
	memInterface[i]=0xFF;
      break;
      
    case SCC_plusmode:
      //32 bytes waveform five
      for (int i=0 ; i<=31 ; i++)
	memInterface[128+i]=wave[4][i];
      //32 bytes Frequency Volume data
      getFreqVol(0xA0);
      //32 bytes deform register
      getDeform(0xC0);
      //no function area
      for (int i=0xE0 ; i<=0xFF ; i++)
	memInterface[i]=0xFF;
  }
}

void SCC::getDeform(byte offset)
{
  //32 bytes deform register
  for (int i=0 ; i<=0x1F ; i++)
    memInterface[ i + offset ]=DeformationRegister;
}

void SCC::getFreqVol(byte offset)
{
  int i;
  byte value;
  //32 bytes Frequency Volume data
  for (i=0;i<10;i++){
    value=i&1?freq[i>>1]>>8:freq[i>>1]&0xff;
    // Take care of the mirroring
    memInterface[i] = value;
    memInterface[i+16] = value;
  }
  for (i=0;i<5;i++){
    memInterface[i+10] = volume[i];
    memInterface[i+10+16] = volume[i];
  }
  memInterface[i+15]=ch_enable;
  memInterface[i+31]=ch_enable;
}


void SCC::writeMemInterface(byte address, byte value, const EmuTime &time)
{
  //short regio;
  short waveborder;
  int i;

  Mixer::instance()->updateStream(time);

  // waveform info
  // note: extra 32 bytes in SCC+ mode
  waveborder=(currentChipMode == SCC_plusmode)?0xa0:0x80;

  if (address<waveborder) {
    //PRT_DEBUG("SCC: in waveborder"<<(int)address);
    int ch = address>>5;
    //Don't know why but japanese doesn't change wave when noise is enabled ??
    //if (!rotate[ch]) {
    wave[ch][address&0x1f] = value;
    volAdjustedWave[ch][address&0x1f] = (short)value * (short)volume[ch];
    if ((currentChipMode != SCC_plusmode)&&(ch==3)){
      wave[4][address&0x1f] = value;
      volAdjustedWave[4][address&0x1f] = (short)value * (short)volume[4];
      //PRT_DEBUG("SCC: waveform 5");
      if (currentChipMode == SCC_Compatible)
	memInterface[address+64] = value ;
    }
    //} Doing this would conflict with wave5=4 meminterface from 2 lines above
    // TODO: Need to figure this noise thing out
    memInterface[address] = value ;
    return;
  }
  switch (currentChipMode ){
  case SCC_Real:
    if ( address < 0xA0 ){
      setFreqVol(value,address-0x80);
    } else if (address >= 0xE0){
      setDeformReg(value);
    }
    break;
  case SCC_Compatible:
    if ( address < 0xA0 ){
      setFreqVol(value,address-0x80);
      memInterface[address | 0x10] = value ;
      memInterface[address & 0xEF] = value ;
    } else if ((address >= 0xC0) && (address < 0xE0)){
      setDeformReg(value);
      for (i=0xC0;i<=0xE0;i++) memInterface[i] = value ;
    }
    break;
  case SCC_plusmode:
    if ( address < 0xC0 ){
      setFreqVol(value,address-0xA0);
      memInterface[address | 0x10] = value ;
      memInterface[address & 0xEF] = value ;
    } else if ((address >= 0xC0) && (address < 0xE0)){
      setDeformReg(value);
      for (i=0xC0;i<=0xE0;i++) memInterface[i] = value ;
    }
  }
}

void SCC::setDeformReg(byte value)
{
  //deformation register
  //PRT_DEBUG("SCC: setDeformReg");
  DeformationRegister = value;
  cycle_4bit = value&1;
  cycle_8bit = value&2;
  refresh = value&32;
  /* Code taken from the japanese guy
    Didn't take time to integrate in my method so far
    According to sean these bits should produce noise on the channels

  if (value&64)
    for(int ch=0;ch<5;ch++) 
      rotate[ch] = 0x1F;
  else 
    for(int ch=0;ch<5;ch++) 
      rotate[ch] = 0;
  if (value&128)
    rotate[3] = rotate[4] = 0x1F;
    */
}

void SCC::setFreqVol(byte value, byte address)
{
  if (address>16) address-=16; //regio is twice visible
  //PRT_DEBUG("SCC: setFreqVol(value " <<(int)value<<", address "<<(int)address<<")" );

  if (address<0x0A) {
    int ch = (address >> 1);
    if (address&1) 
      freq[ch] = ((value&0xf)<<8) | (freq[ch]&0xff);
    else 
      freq[ch] = (freq[ch]&0xf00) | (value&0xff);
    if (refresh)
      count[ch] = 0;
    unsigned frq = freq[ch];
    if (cycle_8bit)
      frq &= 0xff;
    if (cycle_4bit)
      frq >>= 8; 
    if (frq <= 8)
      incr[ch] = 0;
    else
      incr[ch] = (2<<GETA_BITS)/(frq+1);
  }
  else if (address<0x0F) {
    byte channel=address-0x0a;
    volume[channel] = value&0xf;
    for (int i=0;i<32;i++)
	  volAdjustedWave[channel][i] = ((((short)(wave[channel][i]) * (short)volume[channel])));
    checkMute();
  }
  else if (address==0x0F) {
    PRT_DEBUG("SCC: changing ch_enable to "<<(int)value);
    ch_enable = value&31;
    checkMute();
  }
}

/*
void SCC::start()
{
	//PRT_DEBUG ("Starting " << getName());
	// default implementation same as reset
	reset();
}
void SCC::stop()
{
	//PRT_DEBUG ("Stopping " << getName());
}
void SCC::reset(const EmuTime &time)
{
  reset();
}
*/

void SCC::reset()
{
	//PRT_DEBUG ("Resetting " << getName());

	currentChipMode=SCC_Real;
	DeformationRegister = 0;

	for(int i=0; i<5; i++) {
		for(int j=0; j<32 ; j++) {
			wave[i][j] = 0;
			volAdjustedWave[i][j] = 0;
		}
		count[i] = 0;
		freq[i] = 0;
		//phase[i] = 0;
		volume[i] = 0;
		//offset[i] = 0;
		//rotate[i] = 0;
	}
	enable = 1;
	ch_enable = 0xff;

	cycle_4bit = false;
	cycle_8bit = false;
	refresh = false;

	out = 0;

	sccstep =  (unsigned)((1<<31)/(CLOCK_FREQ/2));
	scctime = 0;
}

void SCC::setSampleRate(int sampleRate)
{
	realstep = (unsigned)((1<<31)/sampleRate);
}

void SCC::setInternalVolume(short maxVolume)
{
	// TODO
}

int* SCC::updateBuffer(int length)
{
  int* buf = buffer;
  int mask;
  int oldlength=length;

  //Clear buffer first
  while (length){
    *(buf++)=0;
    length--;
  };

  length=oldlength;
  buf = buffer;
  mask=1;
  for(int i=0; i<5; i++,mask<<=1 ) {
    if (ch_enable & mask) { // ((ch_enable>>i)&1)
      while (length) {
	out = volAdjustedWave[i][(count[i]>>(GETA_BITS))&0x1f];
	// Simple rate converter
	while (realstep > scctime) {
	  scctime += sccstep;
	  //mix all the SCC soundchannels values into an average
	  count[i] += incr[i];
/*testing if really needed
	  if(count[i] & (1<<(GETA_BITS+5))){ 
	    count[i] &= ((1<<(GETA_BITS+5))-1);
	    out += volAdjustedWave[i][(count[i]>>(GETA_BITS))&0x1f];
	    out >>= 1;
	  }
*/
	}
	scctime -= realstep;
	out+= (*buf);
	//out >>= 1;
	*(buf++) = out;
	length--;
      }
    // Set ready to mix next channel
    buf = buffer;
    length=oldlength;
    }
  }
  return buffer;
}

void SCC::checkMute()
{
  unsigned hasSound=0;
  if ( ch_enable == 0 ){
    PRT_DEBUG("SCC: setInternalMute since ch_enable == 0 ");
    setInternalMute(true);
    return;
  }
  // If there is a channel is enabled and the volume is zero...
  for (byte i=0;i<5;i++){
    if (volume[i]){
      hasSound|=( (1<<i) & ch_enable ) ;
      //if all samples are the same then you want here the sample neither
      //This optimisation will probably take more time to calculate then it will save later on
    }
  };
  if (hasSound){
    PRT_DEBUG("SCC+: setInternalMute(false); ");
	setInternalMute(false);
  }else{
    PRT_DEBUG("SCC+: setInternalMute(true); ");
	setInternalMute(true);
  };
  //setInternalMute(false);
}
