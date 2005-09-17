// $Id$

#include <jack/jack.h>
#include "CassettePort.hh"
#include "CassetteJack.hh"
#include "Semaphore.hh"

#include <cmath>
typedef jack_default_audio_sample_t sample_t;
static const int buf_fac=4;

// some callback functions for Jack
extern "C" {
    int process_callback(jack_nframes_t nframes, void* arg)
    {
		return ((openmsx::CassetteJack*)arg)->jackCallBack(nframes);
    }
    int srate_callback(jack_nframes_t nframes, void* arg)
    {
		return ((openmsx::CassetteJack*)arg)->srateCallBack(nframes);
    }
    int bufsize_callback(jack_nframes_t nframes, void* arg)
    {
		return ((openmsx::CassetteJack*)arg)->bufsizeCallBack(nframes);
    }
    void shutdown_callback(void* arg)
    {
		((openmsx::CassetteJack*)arg)->shutdownCallBack();
    }
}

namespace openmsx {
class BlockFifo
{
public:
	BlockFifo(size_t numblocks_, size_t blocksize_);
	~BlockFifo();
	bool hasRdBlock() const;
	sample_t * getRdBlock();
	void advanceRd();
	bool hasWrBlock() const;
	sample_t * getWrBlock();
	void advanceWr();
private:   
	size_t numBlocks, blockSize;
	size_t readIndex, writeIndex;
	Semaphore readSem, writeSem;
	int * valid;
	sample_t *data, *readP, *writeP;
} ;

BlockFifo::BlockFifo(size_t numblocks, size_t blocksize)
	: numBlocks(numblocks), blockSize(blocksize),
	  readSem(0), writeSem(numblocks)
{
	valid= new int[numBlocks];
	data= new sample_t[numBlocks*blockSize];
	for (size_t i=0 ; i<numBlocks ; ++i)
		valid[i]=0;
	readIndex=writeIndex=0;
	readP=writeP=0;
	}
BlockFifo::~BlockFifo()
{
	delete data;
	delete valid;
}
bool
BlockFifo::hasRdBlock() const
{
	return (bool)valid[readIndex];
}
sample_t *
BlockFifo::getRdBlock()
{
	if (0==readP) {
		readP=data+readIndex*blockSize;
		readSem.down();
	}
	return readP;
}
void 
BlockFifo::advanceRd()
{
	assert(valid[readIndex]);
	valid[readIndex++]=0;
	readP=0;
	writeSem.up();
	if (readIndex==numBlocks) 
		readIndex=0;
}
bool
BlockFifo::hasWrBlock() const
{
	return 0==valid[writeIndex];
}
sample_t *
BlockFifo::getWrBlock()
{
	if (0==writeP) {
		writeP=data+writeIndex*blockSize;
		writeSem.down();
	}
	return writeP;
}
void 
BlockFifo::advanceWr()
{
	assert(!valid[writeIndex]);
	valid[writeIndex++]=1;
	writeP=0;
	readSem.up();
	if (writeIndex==numBlocks) 
		writeIndex=0;
}


static void 
fill_buf(BlockFifo * bf, size_t &sampcnt, size_t bufsize,
		 size_t length, sample_t x, bool * zombie)
{

	int len,j; 
	size_t i; 
	sample_t *p;

	i=0;
	while (i<length && !*zombie) {
		p=bf->getWrBlock()+sampcnt;
		len=(length-i<bufsize-sampcnt)?length-i:(bufsize-sampcnt);
		for (j=0 ; j<len ; ++j)
			*p++=x;
		i+=len;
		sampcnt+=len;
		assert(sampcnt<=bufsize);
		if (bufsize==sampcnt) {
			bf->advanceWr();
			sampcnt=0;
		}   
	}
}

CassetteJack::CassetteJack(Scheduler & scheduler)
	: Schedulable(scheduler), bf_in(0), bf_out(0)
{
	running=false, zombie=false, part=0.0;
}
CassetteJack::~CassetteJack()
{
	if (pendingSyncPoint())
		removeSyncPoint();
	if (getConnector())
		getConnector()->unplug(basetime+timestep);
}
void
CassetteJack::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// TODO emit QT4-signal?
}
short
CassetteJack::readSample(const EmuTime& time)
{
	double index_d;
	size_t index_i;

	if (!running || 0==jack_port_connected(cmtin)) 
		return 0;
	/* ASSUMPTION: openmsx is blocked here we need not worry about the
	   readpointer being advanced during this function */
	index_d=(time-basetime).toDouble()*samplerate;
	index_i=(size_t)floor(index_d);
	/* we use a simple piecewise constant 'interpolation' for now */
	return short(32767*bf_in->getRdBlock()[index_i]);
}

void 
CassetteJack::setSignal(bool output, const EmuTime &time)
{
	double samples=(time-basetime).toDouble()*samplerate;
	size_t len = size_t(floor(samples))-sampcnt;
	if ((time-prevtime).toDouble() < 0.75/4800.0) 
		printf("puls: %g seconde (%f/1200)\n", 
			   (time-prevtime).toDouble(), (time-prevtime).toDouble()*1200.0);
	prevtime=time;
	assert(len<=bufsize);
	if (len) { // likely; we loose pulses if 0==len
		double edge = ((part>=0.0)
					   ? part + (_output ? 1.0-part : -1.0+part)
					   : part + (_output ? 1.0+part : -1.0-part));
		fill_buf(bf_out, sampcnt, bufsize, 1, edge, &zombie);
		if (--len)
			fill_buf(bf_out, sampcnt, bufsize, 
					 len, _output ? 1.0 : -1.0, &zombie);
	}
	_output=output;
	part=output ? samples-(double)sampcnt : -samples+(double)sampcnt;
}

const std::string &
CassetteJack::getName() const
{
	static const std::string name("cassettejack");
	return name;
}
const std::string &
CassetteJack::getDescription() const
{
	static const std::string
		desc("allows to connect an external recording program using jack audio connection kit");
	return desc;
}
void
CassetteJack::plugHelper(Connector* connector, const EmuTime& time)
{
	// TODO: handle errors gracefully
	self=jack_client_new ("openmsx");
	assert(self); 
	samplerate=jack_get_sample_rate(self);
	cmtout=jack_port_register (self,"out",JACK_DEFAULT_AUDIO_TYPE,
							   JackPortIsOutput|JackPortIsTerminal, 0);
	cmtin=jack_port_register (self,"in",JACK_DEFAULT_AUDIO_TYPE,
							  JackPortIsInput, 0);
	assert(cmtin && cmtout);
	bufsize=jack_get_buffer_size(self);
	jack_set_buffer_size_callback (self,bufsize_callback,this);
	bf_in= new BlockFifo(buf_fac,bufsize);
	bf_out= new BlockFifo(buf_fac,bufsize);
	assert(bf_in && bf_out);
	_output=((CassettePortInterface*)connector)->lastOut();
	basetime=prevtime=time;
	sampcnt=0;
	timestep=EmuDuration((double)bufsize/samplerate);
	setSyncPoint(time+timestep);
	jack_set_sample_rate_callback (self,srate_callback,this);
	jack_set_process_callback(self, process_callback, this);

	jack_on_shutdown(self, shutdown_callback, this);
	zombie=false;
	// prefill buffers to avoid glitches
	fill_buf(bf_out, sampcnt, bufsize, bufsize*(buf_fac/2), 0.0, &zombie);
	fill_buf(bf_in, sampcnt, bufsize, bufsize*(buf_fac/2), 0.0, &zombie);
	if (0==jack_activate(self))
		running=true;
	else
		zombie=true;
}
void
CassetteJack::unplugHelper(const EmuTime& time)
{
	// TODO give the other jack-end the cance to finish reading what has
	// been written
	running=false;
	if (0) { // should we do this ?
		executeUntil(time,0);
		running=false;
		while (bf_out->hasRdBlock())
			/* wait */;
	}
	jack_deactivate(self);
	// TODO? postpone until destructor
	//    if (old_rb_out)
	//      jack_ringbuffer_free(old_rb_out);
	//    if (old_rb_in)
	//      jack_ringbuffer_free(old_rb_in);
	jack_port_unregister(self, cmtin);
	jack_port_unregister(self, cmtout);
	jack_client_close(self);
	delete bf_out;
	delete bf_in;
}
void 
CassetteJack::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (!running)
		return;
	if (zombie) {
		getConnector()->unplug(time);
		return;
	}
	// ASSUMPTION (valid at time of writing): the time we get from the
	// scheduler is the same as the one we passed to setSyncPoint, so we
	// know we shopuld finish our bufsize samples.
	// synchronise bf_out
	assert((time-basetime).toDouble()*samplerate > bufsize-1);
	assert((time-basetime).toDouble()*samplerate < bufsize+1);
	size_t len = bufsize-sampcnt;
	if (len) { // likely
		double edge = (part>=0.0)
			? part + (_output ? 1.0-part : +part-1.0)
			: part + (_output ? 1.0+part : -part-1.0) ;
		fill_buf(bf_out, sampcnt, bufsize, 1, edge, &zombie);
		if (--len)
			fill_buf(bf_out, sampcnt, bufsize, 
					 len, _output ? 1.0 : -1.0, &zombie);
	}
	// synchronise bf_in 
	bf_in->getRdBlock(); // make sure there is a block to read
	bf_in->advanceRd(); // and skip it
	basetime=time;
	part=0.0;
	assert(0==sampcnt);
	setSyncPoint(time+timestep);
}
int
CassetteJack::jackCallBack(jack_nframes_t nframes)
{
	char *src,*dest;

	if (0==nframes) return 0;
	if (!running) {
		puts("not running");
		return 1;
	}
	// produce audio data for out
	dest=(char*) jack_port_get_buffer(cmtout,nframes);
	src=(char*) bf_out->getRdBlock();
	memcpy(dest,src,nframes*sizeof(sample_t));
	bf_out->advanceRd();
	// receive data from cmtin
	src=(char*) jack_port_get_buffer(cmtin,nframes);
	dest=(char*) bf_in->getWrBlock();
	memcpy(dest,src,nframes*sizeof(sample_t));
	bf_in->advanceWr();
	return 0;
}
int
CassetteJack::srateCallBack(jack_nframes_t newrate)
{
	samplerate=newrate;
	timestep=EmuDuration((double)bufsize/(samplerate));
	return 0;
}

int
CassetteJack::bufsizeCallBack(jack_nframes_t /*newsize*/)
{
	// it may be possible to stay connected duringa change in buffersize,
	// but we don't even try
	jack_deactivate(self);
	zombie=true;
	// prevent deadlock
	if (!bf_in->hasRdBlock()) {
		bf_in->getWrBlock();
		bf_in->advanceWr();
	}
	if (!bf_out->hasWrBlock()) {
		bf_out->getRdBlock();
		bf_out->advanceRd();
	}
	return 0;
}
void
CassetteJack::shutdownCallBack()
{
	zombie=true;
}
const std::string &
CassetteJack::schedName() const
{
	static const std::string name("CassetteJack");
	return name;
}

} // namespace openmsx
