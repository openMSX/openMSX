// $Id$

#include "DACSound16S.hh"
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "RealTime.hh"


namespace openmsx {

const float DELAY = 0.08;	// TODO tune


DACSound16S::DACSound16S(const string& name_, const string& desc_,
                         short volume, const EmuTime& time)
	: name(name_), desc(desc_),
	  cpu(MSXCPU::instance()),
	  realTime(RealTime::instance())
{
	lastValue = lastWrittenValue = 0;
	nextTime = EmuTime::infinity;
	reset(time);
	
	int bufSize = Mixer::instance().registerSound(this,
	                                               volume, Mixer::MONO);
	buffer = new int[bufSize];
}

DACSound16S::~DACSound16S()
{
	Mixer::instance().unregisterSound(this);
	delete[] buffer;
}

const string& DACSound16S::getName() const
{
	return name;
}

const string& DACSound16S::getDescription() const
{
	return desc;
}

void DACSound16S::setVolume(int newVolume)
{
	volume = newVolume;
}

void DACSound16S::setSampleRate(int sampleRate)
{
	oneSampDur = 1.0 / sampleRate;
}

void DACSound16S::reset(const EmuTime& time)
{
	writeDAC(0, time);
}

void DACSound16S::writeDAC(short value, const EmuTime& time)
{
	if (value == lastWrittenValue) {
		return;
	}
	lastWrittenValue = value;
	if ((value != 0) && (isInternalMuted())) {
		EmuDuration delay = realTime.getEmuDuration(DELAY);
		lastTime = time - delay;
		setInternalMute(false);
	}
	samples.push_back(Sample((volume * value) >> 15, time));
	if (nextTime == EmuTime::infinity) {
		nextTime = time;
	}
}

inline int DACSound16S::getSample(const EmuTime& time)
{
	while (nextTime < time) {
		assert(!samples.empty());
		lastValue = samples.front().value;
		samples.pop_front();
		if (samples.empty()) {
			nextTime = EmuTime::infinity;
			if (lastWrittenValue == 0) {
				setInternalMute(true);
			}
		} else {
			nextTime = samples.front().time;
		}
	}
	return lastValue;
}

int* DACSound16S::updateBuffer(int length)
{
	if (isInternalMuted()) {
		return NULL;
	}
	
	EmuTime now = cpu.getCurrentTimeUnsafe(); // can be one instruction off
	assert(lastTime <= now);
	EmuDuration total = now - lastTime;

	float realDuration = length * oneSampDur;
	EmuDuration duration1 = realTime.getEmuDuration(realDuration);
	if ((lastTime + duration1) > now) {
		duration1 = total;
	}
	
	EmuDuration delay = realTime.getEmuDuration(DELAY);
	if (now < (lastTime + delay)) {
		delay = total;
	}

	uint64 a = duration1.length();
	uint64 b = delay.length();
	uint64 c = total.length();
	EmuDuration duration = ((a + b) != 0) ?
	                  EmuDuration((uint64)((c * a) / (length * (a + b)))) :
	                  EmuDuration((uint64)0);

	int* buf = buffer;
	while (length--) {
		lastTime += duration;
		*(buf++) = getSample(lastTime);
	}
	return buffer;
}

} // namespace openmsx

