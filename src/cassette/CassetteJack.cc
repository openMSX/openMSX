// $Id$

#include <jack/jack.h>
#include "CassettePort.hh"
#include "CassetteJack.hh"
#include "Semaphore.hh"

#include <cmath>
#include <unistd.h> // for getpid()

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
		 size_t length, sample_t x, bool * zombie,
		 sample_t & last_out, sample_t & last_sig)
{
	static const sample_t R=1008.0/1024.0;
	int len,j;
	size_t i;
	sample_t *p,y;

	i=0;
	y=last_out+(x-last_sig)/R;
	while (i<length && !*zombie) {
		p=bf->getWrBlock()+sampcnt;
		len=(length-i<bufsize-sampcnt)?length-i:(bufsize-sampcnt);
		for (j=0 ; j<len ; ++j)
			*p++=(y*=R);
		i+=len;
		sampcnt+=len;
		assert(sampcnt<=bufsize);
		if (bufsize==sampcnt) {
			bf->advanceWr();
			sampcnt=0;
		}
	}
	last_out=y;
	last_sig=x;
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
	running=false, zombie=false, partialInterval=partialOut=0.0;
}

CassetteJack::~CassetteJack()
{
	if (getConnector()) {
		getConnector()->unplug(basetime+timestep);
	}
}

void CassetteJack::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// TODO emit QT4-signal?
}

short CassetteJack::readSample(const EmuTime& time)
{
	if (!running || 0==jack_port_connected(cmtin))
		return 0;
	sample_t *p=bf_in->getRdBlock();
	double res;

	/* ASSUMPTION: openmsx is blocked here we need not worry about the
	   readpointer being advanced during this function */
	double index_d=(time-basetime).toDouble()*samplerate;
	size_t index_i=(size_t)floor(index_d);
	double labda=index_d-floor(index_d);
	/* piecewise linear interpolation */
	if (0==index_i)
		res=labda*(*p)+(1.0-labda)*last_in;
	else
		res=labda*p[index_i]+(1.0-labda)*p[index_i-1];

	return short(res*32767);
}

void
CassetteJack::setSignal(bool output, const EmuTime &time)
{
	double samples=(time-basetime).toDouble()*samplerate;
	size_t len = size_t(floor(samples))-sampcnt;
	double out = _output ? 0.5 : -0.5;

	prevtime=time;
	assert(len<=bufsize);
	if (len) {
		double edge = partialOut + (1.0-partialInterval) * out;
		fill_buf(bf_out, sampcnt, bufsize, 1, edge,
				 &zombie, last_out, last_sig);
		if (--len)
			fill_buf(bf_out, sampcnt, bufsize,
					 len, out, &zombie, last_out, last_sig);
		partialInterval=samples-sampcnt;
		partialOut=partialInterval*out;
	}
	else {
		partialOut += (samples-(sampcnt+partialInterval))*out;
		partialInterval=samples-sampcnt;
	}
	_output=output;
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
CassetteJack::plugHelper(Connector& connector, const EmuTime& time)
{
	// TODO: handle errors gracefully
	char name[sizeof "omsx-12345."];
	snprintf(name, sizeof(name), "omsx-%hu", (unsigned short) getpid());
	self=jack_client_new (name);
	if (!self) {
		throw PlugException("Unable to initialize Jack client.");
	}
	samplerate=jack_get_sample_rate(self);
	cmtout=jack_port_register (self,"casout",JACK_DEFAULT_AUDIO_TYPE,
							   JackPortIsOutput|JackPortIsTerminal, 0);
	cmtin=jack_port_register (self,"casin",JACK_DEFAULT_AUDIO_TYPE,
							  JackPortIsInput, 0);
	assert(cmtin && cmtout);
	bufsize=jack_get_buffer_size(self);
	jack_set_buffer_size_callback (self,bufsize_callback,this);
	bf_in= new BlockFifo(buf_fac,bufsize);
	bf_out= new BlockFifo(buf_fac,bufsize);
	assert(bf_in && bf_out);
	_output = static_cast<CassettePortInterface&>(connector).lastOut();
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
	last_in=last_out=0.0;
	last_sig=_output ? 0.5 : -0.5;
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
	// the time we get from the scheduler is the same as the one we
	// passed to setSyncPoint, so we know we should finish our bufsize
	// samples.
	// synchronise bf_out
	assert((time-basetime).toDouble()*samplerate > bufsize-1);
	assert((time-basetime).toDouble()*samplerate < bufsize+1);
	size_t len = bufsize-sampcnt;
	if (len) { // likely
		if (len==bufsize && 0.0==partialInterval
			&& fabs(last_out)<0.0001) { // cut-off
			last_out=0.0;
			fill_buf(bf_out, sampcnt, bufsize,
					 len, 0.0, &zombie);
		}
		else {
			setSignal(_output,time);
		}
	}
	// synchronise bf_in:
	// preserve last sample for interpolation:
	last_in=bf_in->getRdBlock()[bufsize-1];//keep last sample for interpolation
	bf_in->advanceRd(); // release the block for new data
	basetime=time;
	partialInterval=partialOut=0.0;
	assert(0==sampcnt);
	setSyncPoint(time+timestep);
}
int
CassetteJack::jackCallBack(jack_nframes_t nframes)
{
	char *src,*dest;

	if (0==nframes) return 0;
	if (!running) return 1;
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
