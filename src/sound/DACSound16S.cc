// $Id$

#include "DACSound16S.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "DynamicClock.hh"
#include "serialize.hh"

namespace openmsx {

DACSound16S::DACSound16S(string_ref name, string_ref desc,
                         const DeviceConfig& config)
	: SoundDevice(config.getMotherBoard().getMSXMixer(), name, desc, 1)
	, lastWrittenValue(0)
{
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

void DACSound16S::reset(EmuTime::param time)
{
	writeDAC(0, time);
}

void DACSound16S::writeDAC(short value, EmuTime::param time)
{
	int delta = value - lastWrittenValue;
	if (delta == 0) return;
	lastWrittenValue = value;

	BlipBuffer::TimeIndex t;
	getHostSampleClock().getTicksTill(time, t);
	blip.addDelta(t, delta);
}

void DACSound16S::generateChannels(int** bufs, unsigned num)
{
	// Note: readSamples() replaces the values in the buffer. It should add
	//       to the existing values in the buffer. But because there is only
	//       one channel this doesn't matter (buffer contains all zeros).
	if (!blip.readSamples<1>(bufs[0], num)) {
		bufs[0] = nullptr;
	}
}

bool DACSound16S::updateBuffer(unsigned length, int* buffer,
                               EmuTime::param /*time*/)
{
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
		writeDAC(lastValue, getHostSampleClock().getTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(DACSound16S);

} // namespace openmsx

