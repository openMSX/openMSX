// $Id$

#include "DACSound16S.hh"
#include "Mixer.hh"

using std::string;

namespace openmsx {

DACSound16S::DACSound16S(Mixer& mixer, const string& name, const string& desc,
                         const XMLElement& config, const EmuTime& /*time*/)
	: SoundDevice(mixer, name, desc)
{
	lastWrittenValue = 0;
	prevValue = prevA = prevB = 0;
	registerSound(config);
}

DACSound16S::~DACSound16S()
{
	unregisterSound();
}

void DACSound16S::setVolume(int newVolume)
{
	volume = newVolume;
}

void DACSound16S::setSampleRate(int /*sampleRate*/)
{
	// nothing
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

	queue.push_back(Sample(time, (value * volume) >> 15));
	setMute(false);
}

void DACSound16S::updateBuffer(unsigned length, int* buffer,
     const EmuTime& start, const EmuDuration& sampDur)
{
	// purge samples before start
	Queue::iterator it = queue.begin();
	while ((it != queue.end()) && (it->time < start)) ++it;
	if (it != queue.begin()) {
		prevValue = (it - 1)->value;
		queue.erase(queue.begin(), it);
	}

	EmuDuration halfDur = sampDur / 2;
	int64 durA = halfDur.length();
	int64 durB = sampDur.length() - durA;

	EmuTime curr = start;
	while (length--) {
		// 2x oversampling
		EmuTime nextA = curr + halfDur;
		EmuTime nextB = curr + sampDur;

		int64 sumA = 0;
		while (!queue.empty() && (queue.front().time < nextA)) {
			sumA += (queue.front().time - curr).length() * prevValue;
			curr = queue.front().time;
			prevValue = queue.front().value;
			queue.pop_front();
		}
		sumA += (nextA - curr).length() * prevValue;
		curr = nextA;
		int sampA = sumA / durA;

		int64 sumB = 0;
		while (!queue.empty() && (queue.front().time < nextB)) {
			sumB += (queue.front().time - curr).length() * prevValue;
			curr = queue.front().time;
			prevValue = queue.front().value;
			queue.pop_front();
		}
		sumB += (nextB - curr).length() * prevValue;
		curr = nextB;
		int sampB = sumB / durB;

		// 4th order symmetrical FIR filter (1 3 3 1)
		int value = (prevA + sampB + 3 * (prevB + sampA)) / 8;
		*(buffer++) = value;
		prevA = sampA;
		prevB = sampB;
	}

	setMute(queue.empty() && (prevValue == 0));
}

} // namespace openmsx

