// $Id$

#include "CassettePort.hh"
#include "CassetteJack.hh"
#include "Semaphore.hh"
#include "StringOp.hh"
#include <cmath>
#include <unistd.h> // for getpid()
#include <jack/jack.h>

typedef jack_default_audio_sample_t sample_t;
static const int BUF_FAC = 4;

static std::string lastError;

namespace openmsx {

class BlockFifo
{
public:
	BlockFifo(size_t numblocks, size_t blocksize);
	~BlockFifo();

	bool hasRdBlock() const;
	sample_t* getRdBlock();
	void advanceRd();
	bool hasWrBlock() const;
	sample_t* getWrBlock();
	void advanceWr();

private:
	size_t numBlocks, blockSize;
	size_t readIndex, writeIndex;
	Semaphore readSem, writeSem;
	bool* valid;
	sample_t *data, *readP, *writeP;
};

BlockFifo::BlockFifo(size_t numblocks, size_t blocksize)
	: numBlocks(numblocks), blockSize(blocksize)
	, readSem(0), writeSem(numblocks)
{
	valid = new bool[numBlocks];
	data = new sample_t[numBlocks * blockSize];
	for (size_t i = 0; i < numBlocks; ++i) {
		valid[i] = false;
	}
	readIndex = writeIndex = 0;
	readP = writeP = 0;
}

BlockFifo::~BlockFifo()
{
	delete[] data;
	delete[] valid;
}

bool BlockFifo::hasRdBlock() const
{
	return valid[readIndex];
}

sample_t* BlockFifo::getRdBlock()
{
	if (readP == 0) {
		readP = data + readIndex * blockSize;
		readSem.down();
	}
	return readP;
}

void BlockFifo::advanceRd()
{
	assert(valid[readIndex]);
	valid[readIndex++] = false;
	readP = 0;
	writeSem.up();
	if (readIndex == numBlocks) {
		readIndex = 0;
	}
}

bool BlockFifo::hasWrBlock() const
{
	return !valid[writeIndex];
}

sample_t* BlockFifo::getWrBlock()
{
	if (writeP == 0) {
		writeP = data + writeIndex * blockSize;
		writeSem.down();
	}
	return writeP;
}

void BlockFifo::advanceWr()
{
	assert(!valid[writeIndex]);
	valid[writeIndex++] = true;
	writeP = 0;
	readSem.up();
	if (writeIndex == numBlocks) {
		writeIndex = 0;
	}
}


static void fill_buf(BlockFifo* bf, size_t& sampcnt, size_t bufsize,
                     size_t length, sample_t x, bool* zombie,
                     sample_t& last_out, sample_t& last_sig)
{
	static const sample_t R = 1008.0 / 1024.0;

	size_t i = 0;
	sample_t y = last_out + (x - last_sig) / R;
	while (i < length && !*zombie) {
		sample_t* p = bf->getWrBlock() + sampcnt;
		int len = (length - i < bufsize - sampcnt)
		        ? length - i
		        : bufsize - sampcnt;
		for (int j = 0; j < len; ++j) {
			*p++ = (y *= R);
		}
		i += len;
		sampcnt += len;
		assert(sampcnt <= bufsize);
		if (bufsize == sampcnt) {
			bf->advanceWr();
			sampcnt = 0;
		}
	}
	last_out = y;
	last_sig = x;
}

static void fill_buf(BlockFifo* bf, size_t& sampcnt, size_t bufsize,
                     size_t length, sample_t x, bool* zombie)
{
	size_t i = 0;
	while (i < length && !*zombie) {
		sample_t* p = bf->getWrBlock() + sampcnt;
		int len = (length - i < bufsize - sampcnt)
		        ? length - i
		        : bufsize - sampcnt;
		for (int j = 0; j < len; ++j) {
			*p++ = x;
		}
		i += len;
		sampcnt += len;
		assert(sampcnt <= bufsize);
		if (bufsize == sampcnt) {
			bf->advanceWr();
			sampcnt = 0;
		}
	}
}


CassetteJack::CassetteJack(Scheduler& scheduler)
	: Schedulable(scheduler), self(0)
	, cmtin(0), cmtout(0), bf_in(0), bf_out(0)
	, basetime(EmuTime::zero), prevtime(EmuTime::zero)

{
	running = false;
	zombie = false;
	partialInterval = partialOut = 0.0;

	jack_set_error_function(error_callback);
}

CassetteJack::~CassetteJack()
{
	if (getConnector()) {
		getConnector()->unplug(basetime + timestep);
	}
}

void CassetteJack::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// TODO emit QT4-signal?
}

short CassetteJack::readSample(const EmuTime& time)
{
	if (!running || !jack_port_connected(cmtin)) {
		return 0;
	}

	// ASSUMPTION: openmsx is blocked here we need not worry about the
	// readpointer being advanced during this function
	double index_d = (time - basetime).toDouble() * samplerate;
	size_t index_i = (size_t)floor(index_d);
	double lambda = index_d - floor(index_d);
	
	// piecewise linear interpolation
	sample_t* p = bf_in->getRdBlock();
	double res =        lambda  *            p[index_i]                +
	             (1.0 - lambda) * (index_i ? p[index_i - 1] : last_in);
	return short(res * 32767);
}

void CassetteJack::setSignal(bool newOutput, const EmuTime& time)
{
	double samples = (time - basetime).toDouble() * samplerate;
	size_t len = size_t(floor(samples)) - sampcnt;
	double out = output ? 0.5 : -0.5;

	prevtime = time;
	assert(len <= bufsize);
	if (len) {
		double edge = partialOut + (1.0 - partialInterval) * out;
		fill_buf(bf_out, sampcnt, bufsize, 1, edge,
		         &zombie, last_out, last_sig);
		if (--len) {
			fill_buf(bf_out, sampcnt, bufsize,
			         len, out, &zombie, last_out, last_sig);
		}
		partialInterval = samples - sampcnt;
		partialOut = partialInterval * out;
	} else {
		partialOut += (samples - (sampcnt + partialInterval)) * out;
		partialInterval = samples - sampcnt;
	}
	output = newOutput;
}

const std::string& CassetteJack::getName() const
{
	static const std::string name("cassettejack");
	return name;
}

const std::string& CassetteJack::getDescription() const
{
	static const std::string desc(
		"allows to connect an external recording program using "
		"jack audio connection kit");
	return desc;
}

void CassetteJack::plugHelper(Connector& connector, const EmuTime& time)
{
	std::string name = "omsx-" + StringOp::toString(getpid());
	self = jack_client_new(name.c_str());
	if (!self) initError("Unable to initialize jack client");

	cmtout = jack_port_register(self, "casout", JACK_DEFAULT_AUDIO_TYPE,
	                            JackPortIsOutput | JackPortIsTerminal, 0);
	if (!cmtout) initError("Unable to register jack casout port");
	
	cmtin = jack_port_register(self, "casin", JACK_DEFAULT_AUDIO_TYPE,
	                           JackPortIsInput, 0);
	if (!cmtout) initError("Unable to register jack casin port");

	bufsize = jack_get_buffer_size(self);
	jack_set_buffer_size_callback(self, bufsize_callback, this);
	bf_in  = new BlockFifo(BUF_FAC, bufsize);
	bf_out = new BlockFifo(BUF_FAC, bufsize);

	jack_set_sample_rate_callback(self, srate_callback, this);
	srateCallBack(jack_get_sample_rate(self));

	output = static_cast<CassettePortInterface&>(connector).lastOut();
	basetime = prevtime = time;
	sampcnt = 0;
	setSyncPoint(time + timestep);
	jack_set_process_callback(self, process_callback, this);

	jack_on_shutdown(self, shutdown_callback, this);
	zombie = false;

	// prefill buffers to avoid glitches
	fill_buf(bf_out, sampcnt, bufsize, bufsize * (BUF_FAC / 2), 0.0, &zombie);
	fill_buf(bf_in,  sampcnt, bufsize, bufsize * (BUF_FAC / 2), 0.0, &zombie);
	last_in = last_out = 0.0;
	last_sig = output ? 0.5 : -0.5;
	if (jack_activate(self) == 0) {
		running = true;
	} else {
		zombie = true;
	}
}

void CassetteJack::initError(std::string message)
{
	deinit();
	if (!lastError.empty()) {
		message += ": " + lastError;
		lastError.clear();
	}
	throw PlugException(message);
}

void CassetteJack::unplugHelper(const EmuTime& time)
{
	// TODO Should we do this?
	// give the other jack-end the chance to finish reading 
	// what has been written
	if (false) {
		executeUntil(time, 0);
		while (bf_out->hasRdBlock()) {
			// wait
		}
	}

	deinit();
}

void CassetteJack::deinit()
{
	running = false;
	if (self) {
		jack_deactivate(self);
		if (cmtin) {
			jack_port_unregister(self, cmtin);
			cmtin = 0;
		}
		if (cmtout) {
			jack_port_unregister(self, cmtout);
			cmtout = 0;
		}
		jack_client_close(self);
		self = 0;
	}
	delete bf_out;
	delete bf_in;
}


void CassetteJack::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (!running) {
		return;
	}
	if (zombie) {
		getConnector()->unplug(time);
		return;
	}
	// the time we get from the scheduler is the same as the one we
	// passed to setSyncPoint, so we know we should finish our bufsize
	// samples.
	// synchronise bf_out
	assert((time - basetime).toDouble() * samplerate > bufsize - 1);
	assert((time - basetime).toDouble() * samplerate < bufsize + 1);
	size_t len = bufsize - sampcnt;
	if (len) { // likely
		if ((len == bufsize) && (partialInterval == 0.0) &&
		    (fabs(last_out) < 0.0001)) { // cut-off
			last_out = 0.0;
			fill_buf(bf_out, sampcnt, bufsize, len, 0.0, &zombie);
		} else {
			setSignal(output, time);
		}
	}
	// synchronise bf_in:
	// preserve last sample for interpolation:
	last_in = bf_in->getRdBlock()[bufsize - 1];
	bf_in->advanceRd(); // release the block for new data
	basetime = time;
	partialInterval = partialOut = 0.0;
	assert(sampcnt == 0);
	setSyncPoint(time + timestep);
}

const std::string& CassetteJack::schedName() const
{
	static const std::string name("CassetteJack");
	return name;
}

int CassetteJack::process_callback(jack_nframes_t nframes, void* arg)
{
	return ((openmsx::CassetteJack*)arg)->jackCallBack(nframes);
}
int CassetteJack::jackCallBack(jack_nframes_t nframes)
{
	if (nframes == 0) return 0;
	if (!running) return 1;

	// produce audio data for out
	char* dest = (char*)jack_port_get_buffer(cmtout, nframes);
	char* src  = (char*)bf_out->getRdBlock();
	memcpy(dest, src, nframes * sizeof(sample_t));
	bf_out->advanceRd();

	// receive data from cmtin
	src  = (char*)jack_port_get_buffer(cmtin, nframes);
	dest = (char*)bf_in->getWrBlock();
	memcpy(dest, src, nframes * sizeof(sample_t));
	bf_in->advanceWr();
	return 0;
}

int CassetteJack::srate_callback(jack_nframes_t nframes, void* arg)
{
	return ((openmsx::CassetteJack*)arg)->srateCallBack(nframes);
}
int CassetteJack::srateCallBack(jack_nframes_t newrate)
{
	samplerate = newrate;
	timestep = EmuDuration((double)bufsize / samplerate);
	return 0;
}

int CassetteJack::bufsize_callback(jack_nframes_t nframes, void* arg)
{
	return ((openmsx::CassetteJack*)arg)->bufsizeCallBack(nframes);
}
int CassetteJack::bufsizeCallBack(jack_nframes_t /*newsize*/)
{
	// it may be possible to stay connected duringa change in buffersize,
	// but we don't even try
	jack_deactivate(self);
	zombie = true;

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

void CassetteJack::shutdown_callback(void* arg)
{
	((openmsx::CassetteJack*)arg)->shutdownCallBack();
}
void CassetteJack::shutdownCallBack()
{
	zombie = true;
}

void CassetteJack::error_callback(const char* message)
{
	lastError = message;
	PRT_DEBUG("Jack error: " + lastError);
}

} // namespace openmsx
