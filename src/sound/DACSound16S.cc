// $Id$

#include "DACSound16S.hh"
#include "serialize.hh"

namespace openmsx {

DACSound16S::DACSound16S(MSXMixer& mixer, const std::string& name,
                         const std::string& desc, const XMLElement& config)
	: SoundDevice(mixer, name, desc, 1)
	, start(EmuTime::zero) // dummy
{
	lastWrittenValue = 0;
	registerSound(config);
}

DACSound16S::~DACSound16S()
{
	unregisterSound();
}

void DACSound16S::setOutputRate(unsigned sampleRate)
{
	setInputRate(sampleRate);
}

void DACSound16S::reset(const EmuTime& time)
{
	writeDAC(0, time);
}

void DACSound16S::writeDAC(short value, const EmuTime& time)
{
	assert(queue.empty() || (queue.back().time <= time));

	if (value == lastWrittenValue) return;
	lastWrittenValue = value;
	queue.push_back(Sample(time, value));
}

void DACSound16S::generateChannels(int** bufs, unsigned num)
{
	// purge samples before start
	Queue::iterator it = queue.begin();
	while ((it != queue.end()) && (it->time < start)) ++it;
	if (it != queue.begin()) {
		queue.erase(queue.begin(), it);
	}

	// BlipBuffer based implementation
	EmuTime stop = start + sampDur * num;
	while (!queue.empty() && (queue.front().time < stop)) {
		double t = (queue.front().time - start).div(sampDur);
		blip.update(BlipBuffer::TimeIndex(t), queue.front().value);
		queue.pop_front();
	}
	// Note: readSamples() replaces the values in the buffer. It should add
	//       to the existing values in the buffer. But because there is only
	//       one channel this doesn't matter (buffer contains all zeros).
	if (!blip.readSamples(bufs[0], num)) {
		bufs[0] = 0;
	}
}

bool DACSound16S::updateBuffer(unsigned length, int* buffer,
     const EmuTime& start_, const EmuDuration& sampDur_)
{
	// start and sampDur members are only used to pass extra parameters
	// to the generateChannels() method
	start = start_;
	sampDur = sampDur_;
	return mixChannels(buffer, length);
}


template<typename Archive>
void DACSound16S::serialize(Archive& ar, unsigned /*version*/)
{
	// Note: It's ok to NOT serialize a DAC object if you call the
	//       writeDAC() method in some other way during de-serialization.
	//       This is for example done in MSXPPI/KeyClick.
	short lastValue = lastWrittenValue;
	ar.serialize("lastValue", lastValue);
	if (ar.isLoader()) {
		assert(queue.empty());
		writeDAC(lastValue, EmuTime::zero);
	}
}
INSTANTIATE_SERIALIZE_METHODS(DACSound16S);

} // namespace openmsx

