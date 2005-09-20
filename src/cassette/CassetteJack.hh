/* CassetteJack.hh */

#ifndef CASSETTEJACK_HH
#define CASSETTEJACK_HH

#include "CassetteDevice.hh"
#include "Schedulable.hh"
#include "EmuDuration.hh"
#include "EmuTime.hh"
#include <jack/types.h>
#include <jack/ringbuffer.h>

namespace openmsx {

    class BlockFifo;

    /** Allows to connect external programs to the cassette port.
	It needs to inherit from Schedulable because:

	@li it needs to throw away data in the input buffer even if the
        data has not been read
	@li it needs to know the emuTime to get more data from the
        Cassette port to maintain to avoid buffer underruns 
    */

class CassetteJack : public CassetteDevice, public Schedulable
{
public:
    CassetteJack(Scheduler&);
    ~CassetteJack();
    
    //for Cassette Device
    virtual void setMotor(bool status, const EmuTime &time);
    virtual short readSample(const EmuTime &time);
    virtual void setSignal(bool output, const EmuTime &time);
    
    // Pluggable
    virtual const std::string& getName() const;
    virtual const std::string& getDescription() const;
    // Scheduleable
    virtual void executeUntil(const EmuTime& time, int userData) ;
    virtual const std::string& schedName() const;
    // CallBacks for Jack
    int jackCallBack(jack_nframes_t);
    int srateCallBack(jack_nframes_t);
    int bufsizeCallBack(jack_nframes_t);
    void shutdownCallBack();
protected:
    // pluggable 
    virtual void plugHelper(Connector* newConnector, const EmuTime& time);
    virtual void unplugHelper(const EmuTime& time);
    
private:
    jack_client_t * self;
    jack_port_t * cmtout;
    jack_port_t * cmtin;
    BlockFifo *bf_in, *bf_out;
    jack_default_audio_sample_t last_sig, last_out;
    size_t bufsize, sampcnt;
    EmuTime basetime; // last sync with sampletime
    EmuTime prevtime; // last time of setSignal
    EmuDuration timestep; // for scheduling
    double part; // to compute the value of samples between constant parts
    uint32 samplerate;
    bool running, _output, zombie;
} ;
    
} // namespace openmsx
#endif

