/**
 *
 * Emulation of an 8bit unsigned DAC audio device
 *
 */


#include <assert.h>
#include "DACSound.hh"
#include "Mixer.hh"


DACSound::DACSound()
{
  PRT_DEBUG("DAC audio created");
}


DACSound::~DACSound()
{
  PRT_DEBUG("DAC audio destroyed");
}


void DACSound::init()
{
	bufwriteindex=bufreadindex=0;
	DACValue=0;
	DACSample=0;
	setVolume(Mixer::MAX_VOLUME);
	reset();
	int bufSize = Mixer::instance()->registerSound(this);
	buf = new short[bufSize];
	lastChanged=Emutime(CLOCK,0);
}


void DACSound::reset()
{
}


byte DACSound::readDAC(byte value, const Emutime &time)
{
	return DACValue;
}


void DACSound::writeDAC(byte value, const Emutime &time)
{
  int count;
  
  DACValue=value;
  DACSample=volTable[DACValue];
  // change the audiobuffer according to time passed
  if ( bufreadindex != bufwriteindex ){
    // DAC output is not upto current buffer
    // counter from audiobuffer must be set correctly according to current emutime-lastChanged
    delta+=lastChanged.getTicksTill(time);
    // test debug    delta+=400;
    count=(int)(delta/clocksPerSample) ; //TODO check for overflow etc etc
    delta=delta - ( count * clocksPerSample ) ;
    audiobuffer[bufwriteindex].time=count;
    //sample is set in previous call of writeDAC()
  }
  // we could optimise below if we save the same value twice,
  // but we will not do this for the setVolume changes at runtime.
  audiobuffer[++bufwriteindex].time=1;
  audiobuffer[bufwriteindex].sample=DACSample;
  lastChanged=time;
}


void DACSound::setVolume(short newVolume)
{
	// calculate the volume->voltage conversion table
	// The DAC uses 8 bit unsigned wave data
	// 0x00 is mac negative, 0xFF max positive
	// 0x80 is centre (no amplitude)

	double scale = (double) (newVolume / 0x80);	// scale factor for DAC
  	PRT_DEBUG("DAC scale : " << scale );
	short offset = (short)(scale * 0x80);		// is the zero level
  	PRT_DEBUG("DAC offset :" << offset );
	for (int i=0; i<256; i++) {
		volTable[i] = (short)((scale * i) - offset);
	}
	// sample value must be changed
	DACSample=volTable[DACValue];
	// TODO: we also  should change the possible samples of audiobuffer 
	// but for the moment no emutime is past with this call so we will use 
	// getCurrentEmutime once we start implementing
}


void DACSound::setSampleRate (int sampleRate)
{
	// Here we calculate the number of ticks which happen during one sample
	// at the given sample rate.
	clocksPerSample = (int)(CLOCK / sampleRate);
	PRT_DEBUG("Clockticks per samplerate "<<clocksPerSample);
}


short* DACSound::updateBuffer(int length)
{
  short* buffer = buf;
  int nrsamples;
  short sample;
  while (length >0 ){
    if ( bufreadindex == bufwriteindex ){
      // up to date so output is DACSample
      for (;length>0;length--,buffer++) *buffer=DACSample;
    } else {
      // update from bufreadindex
      nrsamples = audiobuffer[bufreadindex].time;
      sample = audiobuffer[bufreadindex].sample;

      if ( nrsamples <= length ){
	length-=nrsamples;
	for (;nrsamples>0;nrsamples--,buffer++)*buffer=sample;
      } else {
	audiobuffer[bufreadindex].time-=length;
	for (;length>0;length--,buffer++)*buffer=sample;
      }
      
      bufreadindex++;

    }
  }
  return buf;
}
